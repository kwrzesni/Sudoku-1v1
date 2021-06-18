#include "Server.h"
#include "Logger.h"
#include "NetworkException.h"
#include "IOMode.h"
#include <fstream>

Server::Server(const std::string& configPath)
{
	std::ifstream in(configPath);
	in >> serverConfig;
	serverEndpoint = IPEndpoint{ std::string(serverConfig["IP"]).c_str(), serverConfig["PORT"] };
}

void Server::Start()
{
	socket.create(TransmissionType::unicast, serverConfig["TIMEOUT"]);
	socket.bind(serverEndpoint);
	serverThread = std::make_unique<std::thread>(&Server::Listen, this, 5);
	LOG << "server on " + serverEndpoint.toString() + " successfuly started\n";
}


Server::~Server()
{
	alive.store(false);
	if (serverThread)
	{
		serverThread->join();
		serverThread.reset();
	}
}

void Server::Listen(int backlog)
{
	while (alive.load())
	{
		if (socket.listen(backlog) == Result::success)
		{
			ServerSocket respondingSocket;
			if (socket.accept(respondingSocket) == Result::success)
			{
				LOG << "Added client on " + respondingSocket.toString() << '\n';
				std::thread clientThread(&Server::StartConnection, this, std::move(respondingSocket));
				clientThread.detach();
			}
		}
	}
}

void Server::StartConnection(ServerSocket&& socket)
{
	std::string name;
	if (socket.setIOMode(IOMode::fionbio, 0ul) != Result::success)
	{
		int errorCode = WSAGetLastError();
		throw NETWORK_EXCEPTION(errorCode);
	}
	Json data;
	Result result = Result::success;
	if (socket.recieveJson(data) == Result::success)
	{
		if (data["type"] != "connect")
		{
			Json respond;
			respond["type"] = "error";
			respond["reason"] = "not connected";
			socket.sendJson(respond);
			return;
		}
		name = data["name"];
		if (HandleConnectionRequest(name, std::move(socket)) != Result::success)
		{
			return;
		}
		WaitForMessages(name);
	}
}

void Server::WaitForMessages(const std::string& name)
{
	mutex.lock();
	User& user = *std::find_if(users.begin(), users.end(), [&name](const User& user) {return user.name == name; });
	mutex.unlock();
	if (user.socket.setSocketOption(SocketOption::SO_RecieveTimeout, 0ul) != Result::success)
	{
		int errorCode = WSAGetLastError();
		throw NETWORK_EXCEPTION(errorCode);
	}
	Json data;
	Result result = Result::success;
	while (result == Result::success && alive.load())
	{
		if ((result = user.socket.recieveJson(data)) == Result::success)
		{
			result = HandleMessage(user, data);
		}
	}
	RemoveUser(user);
}

void Server::SendUsers(User& user)
{
	std::vector<std::string> roomIds;
	std::vector<std::string> names;
	roomIds.reserve(users.size());
	names.reserve(users.size());
	for (const auto& u : users)
	{
		roomIds.push_back(std::to_string(u.room ? u.room->GetId() : 0));
		names.push_back(u.name);
	}

	Json message;
	message["type"] = "usersList";
	message["roomIds"] = roomIds;
	message["names"] = names;
	user.socket.sendJson(message);
}

void Server::AddUser(User && user)
{
	users.emplace_front(std::move(user));
	BroadcastAddUser(users.front());
}

void Server::SendRooms(User & user)
{
	std::vector<std::string> ids;
	std::vector<std::string> hosts;
	std::vector<std::string> guests;
	std::vector<bool> locks;
	ids.reserve(users.size());
	hosts.reserve(users.size());
	guests.reserve(users.size());
	locks.reserve(users.size());
	for (const auto& r : rooms)
	{
		ids.push_back(std::to_string(r.GetId()));
		hosts.push_back(r.GetHost().name);
		guests.push_back(r.GetGuest().name);
		locks.push_back(r.IsLocked());
	}

	Json message;
	message["type"] = "roomsList";
	message["ids"] = ids;
	message["hosts"] = hosts;
	message["guests"] = guests;
	message["locks"] = locks;
	user.socket.sendJson(message);
}

void Server::CreateRoom(User & user)
{
	Room room(user.name, &user.socket);
	BroadcastAddRoom(room);
	rooms.emplace_back(std::move(room));
	user.room = &rooms.back();
	Json message;
	message["type"] = "changeUser";
	message["change"] = "roomId";
	message["name"] = user.name;
	message["roomId"] = std::to_string(user.room->GetId());
	BroadcastMessage(message);

	message = Json{};
	message["type"] = "join";
	message["roomId"] = room.GetId();
	message["host"] = user.name;
	message["guest"] = "";
	message["as"] = "host";
	message["locked"] = room.IsLocked();
	message["difficulty"] = room.GetDifficulty();
	user.socket.sendJson(message);
}

void Server::RemoveRoom(Room & room)
{
	BroadcastRemoveRoom(room);
	rooms.erase(FindRoom(room.GetId()));
}

void Server::JoinRoom(User& user, int roomId)
{
	auto it = FindRoom(roomId);
	if (it != rooms.end())
	{
		Room& room = *it;
		if (!room.GetGuest())
		{
			room.SetGuest(user.name, &user.socket);
			user.room = &room;
			Json message;
			message["type"] = "join";
			message["roomId"] = roomId;
			message["as"] = "guest";
			message["host"] = room.GetHost().name;
			message["guest"] = user.name;
			message["locked"] = room.IsLocked();
			message["difficulty"] = room.GetDifficulty();
			user.socket.sendJson(message);
			
			message = Json{};
			message["type"] = "changeRoom";
			message["change"] = "guest";
			message["roomId"] = std::to_string(roomId);
			message["guest"] = user.name;
			BroadcastMessage(message);

			message = Json{};
			message["type"] = "changeUser";
			message["change"] = "roomId";
			message["name"] = user.name;
			message["roomId"] = std::to_string(roomId);
			BroadcastMessage(message);
		}
	}
}

void Server::LockRoom(int roomId, bool locked)
{
	auto it = FindRoom(roomId);
	if (it != rooms.end())
	{
		Room& room = *it;
		room.SetLock(locked);

		Json message;
		message["type"] = "changeRoom";
		message["change"] = "lock";
		message["roomId"] = std::to_string(roomId);
		message["lock"] = locked;
		BroadcastMessage(message);
	}
}

void Server::QuitRoom(User & user)
{
	if (user.room)
	{
		if (user.room->GetGuest())
		{
			Json message;
			if (user.room->GetGuest().name == user.name)
			{
				message["type"] = "changeRoom";
				message["roomId"] = std::to_string(user.room->GetId());
				message["change"] = "guest";
				message["guest"] = "";
				user.room->SetGuest("", nullptr);
			}
			else
			{
				message["type"] = "changeRoom";
				message["roomId"] = std::to_string(user.room->GetId());
				message["change"] = "host";
				message["host"] = user.room->GetGuest().name;
				user.room->ChangeGuestToHost();
			}
			BroadcastMessage(message);
		}
		else
		{
			RemoveRoom(*user.room);
		}
		user.room = nullptr;
		Json message;
		message["type"] = "changeUser";
		message["change"] = "roomId";
		message["name"] = user.name;
		message["roomId"] = "0";
		BroadcastMessage(message);

		message = Json{};
		message["type"] = "quit";
		user.socket.sendJson(message);
	}
}

void Server::ChangeRoomDifficulty(Room & room, int difficulty)
{
	room.SetDifficulty(difficulty);
	Json message;
	message["type"] = "changeRoom";
	message["change"] = "difficulty";
	message["difficulty"] = difficulty;
	room.GetHost().socket->sendJson(message);
	if (room.GetGuest())
	{
		room.GetGuest().socket->sendJson(message);
	}
}

Server::Rooms::iterator Server::FindRoom(int roomId)
{
	return std::find_if(rooms.begin(), rooms.end(), [&roomId](Room& r) {return r.GetId() == roomId; });
}

void Server::RemoveUser(User& user)
{
	if (user.room)
	{
		if (user.room->GetGuest())
		{
			Json message;
			if (user.room->GetGuest().name == user.name)
			{
				message["type"] = "changeRoom";
				message["roomId"] = std::to_string(user.room->GetId());
				message["change"] = "guest";
				message["guest"] = "";
				user.room->SetGuest("", nullptr);
			}
			else
			{
				message["type"] = "changeRoom";
				message["roomId"] = std::to_string(user.room->GetId());
				message["change"] = "host";
				message["host"] = user.room->GetGuest().name;
				user.room->ChangeGuestToHost();
			}
			BroadcastMessage(message);
		}
		else
		{
			RemoveRoom(*user.room);
		}
	}
	BroadcastRemoveUser(user);
	users.erase(std::find_if(users.begin(), users.end(), [&user](User& u) {return u.name == user.name; }));
}

void Server::BroadcastAddUser(User& user)
{
	Json message;
	message["type"] = "addUser";
	message["roomId"] = user.room ? std::to_string(user.room->GetId()) : "0";
	message["name"] = user.name;

	for (auto& u : users)
	{
		while(u.socket.sendJson(message) != Result::success);
	}
}

void Server::BroadcastRemoveUser(User & user)
{
	Json message;
	message["type"] = "removeUser";
	message["name"] = user.name;

	for (auto& u : users)
	{
		u.socket.sendJson(message);
	}
}

void Server::BroadcastAddRoom(const Room & room)
{
	Json message;
	message["type"] = "addRoom";
	message["id"] = std::to_string(room.GetId());
	message["host"] = room.GetHost().name;
	message["guest"] = room.GetGuest().name;
	message["locked"] = room.IsLocked();

	for (auto& u : users)
	{
		u.socket.sendJson(message);
	}
}

void Server::BroadcastRemoveRoom(const Room& room)
{
	Json message;
	message["type"] = "removeRoom";
	message["id"] = std::to_string(room.GetId());

	for (auto& u : users)
	{
		u.socket.sendJson(message);
	}
}

void Server::BroadcastMessage(const Json & message)
{
	for (auto& u : users)
	{
		u.socket.sendJson(message);
	}
}

Result Server::HandleConnectionRequest(const std::string& name, ServerSocket&& socket)
{
	if (name.size() < MIN_USER_NAME)
	{
		Json respond;
		respond["type"] = "error";
		respond["reason"] = "too short user name";
		socket.sendJson(respond);
		return Result::genericError;
	}
	if (name.size() > MAX_USER_NAME)
	{
		Json respond;
		respond["type"] = "error";
		respond["reason"] = "too long user name";
		socket.sendJson(respond);
		return Result::genericError;
	}
	if (std::find_if(users.cbegin(), users.cend(), [&name](const User& user) {return user.name == name; }) != users.cend())
	{
		Json respond;
		respond["type"] = "error";
		respond["reason"] = "user with this name already exist";
		socket.sendJson(respond);
		return Result::genericError;
	}
	if (users.size() == MAX_NUMBER_OF_USERS)
	{
		Json respond;
		respond["type"] = "error";
		respond["reson"] = "server full";
		socket.sendJson(respond);
		return Result::genericError;
	}
	Json respond;
	respond["type"] = "connect";
	socket.sendJson(respond);

	respond = serverConfig;
	respond["type"] = "serverConfig";
	socket.sendJson(respond);

	User user(name, std::move(socket));
	SendUsers(user);

	SendRooms(user);

	AddUser(std::move(user));
	return Result::success;
}

Result Server::HandleMessage(User & user, const Json & message)
{
	if (message["type"] == "createRoom")
	{
		if (user.room == nullptr)
		{
			CreateRoom(user);
		}
	}
	else if (message["type"] == "join")
	{
		if (user.room == nullptr)
		{
			JoinRoom(user, message["roomId"]);
		}
	}
	else if (message["type"] == "lock")
	{
		if (user.room != nullptr && user.name == user.room->GetHost().name)
		{
			LockRoom(user.room->GetId(), message["lock"]);
		}
	}
	else if (message["type"] == "quit")
	{
		if (user.room != nullptr)
		{
			QuitRoom(user);
		}
	}
	else if (message["type"] == "changeRoom")
	{
		if (user.room != nullptr && user.room->GetHost().name == user.name)
		{
			if (message["change"] == "difficulty")
			{
				ChangeRoomDifficulty(*user.room, message["difficulty"]);
			}
		}
	}
	/*else if (message["type"] == "kick")
	{
		if (user.room != nullptr)
		{
			kickGuestFromRoom();
		}
	}*/
	return Result::success;
}
