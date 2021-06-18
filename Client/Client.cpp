#include "Client.h"
#include "Window.h"
#include <chrono>
#include <sstream>
#include <ws2tcpip.h>

bool isIpAddress(const std::string& ip);

Client::Client(unsigned long timeout)
	: timeout(timeout)
{
	handlers["serverConfig"] = &Window::HandleServerConfig;
	handlers["usersList"] = &Window::HandleUserList;
	handlers["addUser"] = &Window::HandleAddUser;
	handlers["changeUser"] = &Window::HandeChangeUser;
	handlers["removeUser"] = &Window::HandleRemoveUser;
	handlers["roomsList"] = &Window::HandleRoomList;
	handlers["addRoom"] = &Window::HandleAddRoom;
	handlers["removeRoom"] = &Window::HandleRemoveRoom;
	handlers["changeRoom"] = &Window::HandleRoomChange;
	handlers["join"] = &Window::HandleJoin;
	handlers["quit"] = &Window::HandleQuit;
}


void Client::connect(const std::string& name, const std::string& ip, const std::string& portStr)
{
	if (clientThread)
	{
		return;
	}
	if (name.empty())
	{
		MessageBox(nullptr, "empty user name", "Empty user name", MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	if (ip.empty())
	{
		MessageBox(nullptr, "empty ip address", "Empty ip address", MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	if (!isIpAddress(ip))
	{
		std::ostringstream oss;
		oss << "\'" << ip << "\' is not a valid ip address";
		MessageBox(nullptr, oss.str().c_str(), "Invalid server ip address", MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	if (portStr.empty())
	{
		MessageBox(nullptr, "empty port number", "Empty port number", MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	if (!std::all_of(portStr.cbegin(), portStr.cend(), std::isdigit))
	{
		std::ostringstream oss;
		oss << "\'" << portStr << "\' is not a valid port nubmer";
		MessageBox(nullptr, oss.str().c_str(), "Invalid server port", MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	unsigned short port = atoi(portStr.c_str());

	IPEndpoint serverEndpoint(ip.c_str(), port);
	if (serverEndpoint.getIPVersion() == IPVersion::Unknown)
	{
		std::ostringstream oss;
		oss << "\'" << ip << " : " << portStr << "\' is not a valid enpoint";
		MessageBox(nullptr, oss.str().c_str(), "Invalid server enpoint", MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	socket.create(TransmissionType::unicast, timeout);
	clientThread = std::make_unique<std::thread>(&Client::establishConnection, this, name, serverEndpoint);
}

void Client::bind(IPEndpoint endpoint)
{
	socket.create(transmissionType, timeout);
	socket.bind(endpoint);
}

void Client::join()
{
	if (clientThread)
	{
		clientThread->join();
		clientThread.reset();
	}
}

TransmissionType Client::getTransmissionType() const
{
	return transmissionType;
}

void Client::setWindow(Window* wnd)
{
	this->wnd = wnd;
}

bool Client::isConnected() const
{
	return connected.load();
}

void Client::disconnect()
{
	connected.store(false);
	if (clientThread)
	{
		clientThread->join();
		clientThread.reset();
		socket.close();
	}
	wnd->HandleDisconnection();
}

Client::~Client()
{
	connected.store(false);
	join();
}

void Client::establishConnection(const std::string& name, const IPEndpoint& serverEndpoint)
{
	if (socket.connect(serverEndpoint) != Result::success)
	{
		std::ostringstream oss;
		oss << "could not connect to " << serverEndpoint;
		MessageBox(nullptr, oss.str().c_str(), "Connection failed", MB_OK | MB_ICONEXCLAMATION);
		socket.close();
		clientThread->detach();
		clientThread.reset();
		return;
	}
	Json message;
	message["type"] = "connect";
	message["name"] = name;
	Result result = socket.sendJson(message);
	if (result != Result::success)
	{
		std::ostringstream oss;
		oss << "could not send connection request to " << serverEndpoint;
		MessageBox(nullptr, oss.str().c_str(), "Connection failed", MB_OK | MB_ICONEXCLAMATION);
		socket.close();
		clientThread->detach();
		clientThread.reset();
		return;
	}
	result = socket.recieveJson(message);
	if (result != Result::success || message.empty())
	{
		std::ostringstream oss;
		oss << "server did not respond on connection request" << serverEndpoint ;
		MessageBox(nullptr, oss.str().c_str(), "Connection failed", MB_OK | MB_ICONEXCLAMATION);
		socket.close();
		clientThread->detach();
		clientThread.reset();
		return;
	}
	if (message["type"] == "error")
	{
		std::string reason = message["reason"];
		MessageBox(nullptr, reason.c_str(), "Connection failed", MB_OK | MB_ICONEXCLAMATION);
		socket.close();
		clientThread->detach();
		clientThread.reset();
		return;
	}
	if (message["type"] != "connect")
	{
		MessageBox(nullptr, "recieved response of wrong type", "Connection failed", MB_OK | MB_ICONEXCLAMATION);
		socket.close();
		clientThread->detach();
		clientThread.reset();
		return;
	}
	connected.store(true);
	wnd->HandleConnection();
	waitForMessages();
}

void Client::recieveDataT(Json& recievedData, Result& result)
{
	result = socket.recieveJson(recievedData);
}


Result Client::sendMessage(Json& message)
{
	Result result = socket.sendJson(message);
	if (result != Result::success)
	{
		return Result::genericError;
	}
	return Result::success;
}

void Client::waitForMessages()
{
	Json message;
	Result result = Result::success;
	while (connected.load() && (result = socket.recieveJson(message)) != Result::connectionReset)
	{
		if (result == Result::success)
		{
			handleMessage(message);
		}
	}
	if (result == Result::connectionReset)
	{
		wnd->HandleDisconnection();
	}
}

void Client::handleMessage(const Json& message)
{
	auto it = message.find("type");
	if (it == message.end())
	{
		return;
	}
	std::string type = *it;
	handlers[*it](wnd, message);
}

bool isIpAddress(const std::string& ip)
{
	char buff[sizeof(in6_addr)];
	return inet_pton(AF_INET, ip.c_str(), buff) || inet_pton(AF_INET6, ip.c_str(), buff);
}