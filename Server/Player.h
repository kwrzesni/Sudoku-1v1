#pragma once
#include "ServerSocket.h"
#include <string>

struct Player
{
	Player() = default;
	Player(const std::string& name, ServerSocket* socket)
		: name(name), socket(socket)
	{}
	operator bool() const
	{
		return !name.empty();
	}
	std::string name;
	unsigned long points;
	unsigned long lifes;
	ServerSocket* socket = nullptr;
};

