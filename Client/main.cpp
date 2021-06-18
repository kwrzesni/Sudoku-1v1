#include <iostream>
#include "IncludeMe.h"
#include "Client.h"
#include "NetworkException.h"
#include "NetworkEnvironment.h"
#include "BaseException.h"
#include "Application.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	try
	{
		NetworkEnvironment::initialize();
		Application applittion;
		applittion.Go();
	}
	catch (const NetworkException& e)
	{
		MessageBox(nullptr, e.what(), e.getType(), MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}
	catch (const BaseException& e)
	{
		MessageBox(NULL, e.what(), e.GetType(), MB_OK | MB_ICONEXCLAMATION);
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