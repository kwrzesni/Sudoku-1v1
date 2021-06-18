#pragma once
#include "Window.h"
#include "Client.h"

class Application
{
public:
	Application();
	int Go();
	~Application() = default;
private:
	Client client;
	Window wnd;
};

