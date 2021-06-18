#include <iostream>
#include "IncludeMe.h"
#include "Logger.h"
#include "Server.h"
#include "NetworkException.h"
#include "NetworkEnvironment.h"
#include <fstream>

int main(int argc, char* argv[])
{
	try
	{
		std::string configFilePath;
		if (argc < 2)
		{
			configFilePath = configFilePath = "config.json";
		}
		else
		{
			configFilePath = argv[1];
		}
		NetworkEnvironment::initialize();
		Server server(configFilePath);
		server.Start();
		while (1);
	}
	catch (const NetworkException& e)
	{
		MessageBox(nullptr, e.what(), e.getType(), MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}
	catch (const std::exception e)
	{
		MessageBox(nullptr, e.what(), "Standar Exception", MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}
	catch (...)
	{
		MessageBox(nullptr, "No details avalible", "Unknown Exception", MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}
	return 0;
}