#pragma once
#include "ClientSocket.h"
#include "TransmissionType.h"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

class Client
{
public:
	Client(unsigned long timeout);
	void connect(const std::string& name, const std::string& ip, const std::string& portStr);
	void bind(IPEndpoint endpoint);
	void join();
	Result sendMessage(Json& message);
	void waitForMessages();
	void handleMessage(const Json& message);
	TransmissionType getTransmissionType() const;
	void setWindow(class Window* wnd);
	bool isConnected() const;
	void disconnect();
	~Client() noexcept;
private: 
	void establishConnection(const std::string& name, const IPEndpoint& serverEndpoint);
	void recieveDataT(Json& recievedData, Result& result);
private:
	unsigned long timeout;
	ClientSocket socket;
	std::unique_ptr<std::thread> clientThread;
	std::atomic<bool> connected = true;
	static constexpr const TransmissionType transmissionType = TransmissionType::unicast;
	class Window* wnd;
	std::map<std::string, std::function<void(class Window* wnd, const Json& message)>> handlers;
};


