#include "Application.h"

Application::Application()
	:
	client(500lu),
	wnd(800, 600, "Sudoku 1v1", client)
{
	client.setWindow(&wnd);
}

int Application::Go()
{
	while (true)
	{
		if (const std::optional<int> exitCode = Window::ProcesMessage())
			return exitCode.value();
	}
}