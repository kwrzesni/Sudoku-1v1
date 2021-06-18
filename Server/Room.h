#pragma once
#include "Player.h"
#include "ServerSocket.h"

class Room
{
public:
	Room(const std::string& hostName, ServerSocket* socket)
		: id(newRoomId++), host(hostName, socket)
	{}
	int GetId() const
	{
		return id;
	}
	const Player& GetHost() const
	{
		return host;
	}
	const Player& GetGuest() const
	{
		return guest;
	}
	bool IsLocked() const
	{
		return locked;
	}
	int GetDifficulty() const
	{
		return difficulty;
	}
	void SetGuest(const std::string& name, ServerSocket* socket)
	{
		guest.name = name;
		guest.socket = socket;
	}
	void ChangeGuestToHost();
	void SetLock(bool locked)
	{
		this->locked = locked;
	}
	void SetDifficulty(int difficulty)
	{
		this->difficulty = difficulty;
	}
private:
	static int newRoomId;
	int id;
	Player host;
	Player guest;
	bool locked = false;
	int difficulty = 0;
};

