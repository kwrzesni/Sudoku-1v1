#pragma once
#include "ServerSocket.h"
#include "TransmissionType.h"
#include "Room.h"
#include <atomic>
#include <list>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

class Server
{
private:
	struct User
	{
		User(const std::string& name, ServerSocket&& socket)
			: name(name), socket(std::move(socket))
		{}
		User(User&& other)
			: name(std::move(other.name)), socket(std::move(other.socket)), room(other.room)
		{
			other.room = nullptr;
		}

		std::string name;
		ServerSocket socket;
		Room* room = nullptr;
		
	};
	using Users = std::list<User>;
	using Rooms = std::list<Room>;
public:
	Server(const std::string& configPath);
	void Start();
	~Server();
private:
	void Listen(int backlog = 5);
	void StartConnection(ServerSocket&& socket);
	void WaitForMessages(const std::string& name);
	void SendUsers(User& user);
	void AddUser(User&& user);
	void RemoveUser(User& user);
	void SendRooms(User& user);
	void CreateRoom(User& user);
	void RemoveRoom(Room& room);
	void JoinRoom(User& user, int roomId);
	void LockRoom(int roomId, bool locked);
	void QuitRoom(User& user);
	void ChangeRoomDifficulty(Room& room, int difficulty);
	Rooms::iterator FindRoom(int roomId);
	void BroadcastAddUser(User& user);
	void BroadcastRemoveUser(User& user);
	void BroadcastAddRoom(const Room& room);
	void BroadcastRemoveRoom(const Room& room);
	void BroadcastMessage(const Json& message);
	Result HandleConnectionRequest(const std::string& name, ServerSocket&& socket);
	Result HandleMessage(User& user, const Json& message);
private:
	Json serverConfig;
	static constexpr const size_t MIN_USER_NAME = 3;
	static constexpr const size_t MAX_USER_NAME = 10;
	static constexpr const size_t MAX_NUMBER_OF_USERS = 10;
	IPEndpoint serverEndpoint;
	ServerSocket socket;
	std::unique_ptr<std::thread> serverThread;
	std::atomic<bool> alive = true;
	std::mutex mutex;
	Users users;
	Rooms rooms;
};

