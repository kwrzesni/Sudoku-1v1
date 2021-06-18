#include "Window.h"
#include "WindowsThrowMacros.h"
#include <sstream>
#include <windowsx.h>
#include <CommCtrl.h>

std::string GetText(HWND hWnd);

Window::WindowClass Window::WindowClass::wndClass;

const TCHAR* Window::WindowClass::GetName()
{
	return name;
}

HINSTANCE Window::WindowClass::GetInstance()
{
	return wndClass.hInstance;
}

Window::WindowClass::WindowClass()
	:
	hInstance(GetModuleHandle(NULL))
{
	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEX));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpszClassName = GetName();
	wc.hInstance = hInstance;
	wc.lpfnWndProc = SetupWndProc;
	wc.style = CS_OWNDC;
	RegisterClassEx(&wc);
}

Window::WindowClass::~WindowClass()
{
	UnregisterClass(GetName(), hInstance);
}

Window::Window(int width, int height, const TCHAR * name, Client& client)
	:
	width(width),
	height(height),
	client(client)
{
	DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX;
	RECT wndRect;
	wndRect.left = 100;
	wndRect.top = 100;
	wndRect.right = wndRect.left + width;
	wndRect.bottom = wndRect.top + height;
	if(AdjustWindowRect(&wndRect, style, FALSE) == 0)
		throw WND_LAST_EXCEPTION;

	hWnd = CreateWindow(
		WindowClass::GetName(),
		name,
		style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wndRect.right - wndRect.left,
		wndRect.bottom - wndRect.top,
		NULL,
		NULL,
		WindowClass::GetInstance(),
		this
	);

	if (hWnd == NULL)
		throw WND_LAST_EXCEPTION;

	CreateConnectionControls();
	CreateUsersRoomsControls();
	CreateRoomControls();

	SetConnectionControlsVisibilty(true);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
}

void Window::SetTitle(const std::string& title)
{
	if (SetWindowText(hWnd, title.c_str()) == 0)
		throw WND_LAST_EXCEPTION;
}

std::optional<int> Window::ProcesMessage()
{
	MSG msg;
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return msg.wParam;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return {};
}

void Window::SetConnectionControlsVisibilty(bool visible)
{
	connectionControlsVisible = visible;
	int command = visible ? SW_SHOW : SW_HIDE;
	ShowWindow(nameText, command);
	ShowWindow(nameEdit, command);
	ShowWindow(ipAddressText, command);
	ShowWindow(ipAddressEdit, command);
	ShowWindow(portText, command);
	ShowWindow(portEdit, command);
	ShowWindow(connectButton, command);
	SetConnectionControlsLayout();
	InvalidateRect(hWnd, NULL, true);
}

void Window::SetUsersRoomsControlsVisibilty(bool visible)
{
	roomUsersControlsVisible = visible;
	int command = visible ? SW_SHOW : SW_HIDE;
	ShowWindow(createButton, command);
	ShowWindow(joinButton, command);
	ShowWindow(roomsText, command);
	ShowWindow(roomsList, command);
	ShowWindow(usersText, command);
	ShowWindow(usersList, command);
	SetUsersRoomsControlsLayout();
	InvalidateRect(hWnd, NULL, true);
}

void Window::HandleConnection()
{
	SetWindowText(connectButton, "disconnect");
	EnableWindow(createButton, true);
	EnableWindow(joinButton, false);
	SetUsersRoomsControlsVisibilty(true);
}

void Window::HandleDisconnection()
{
	SetWindowText(connectButton, "connect");
	SetUsersRoomsControlsVisibilty(false);
	RemoveAllUsers();
	RemoveAllRooms();
}

void Window::HandleServerConfig(const Json& message)
{
	serverConfig = message;
	serverConfig.erase("type");
}

void Window::HandleUserList(const Json& message)
{
	std::vector<std::string> roomIds = message["roomIds"];
	std::vector<std::string> names = message["names"];
	for (size_t i = 0; i < roomIds.size(); ++i)
	{
		AddUser(roomIds[i], names[i]);
	}
}

void Window::HandleAddUser(const Json& message)
{
	AddUser(message["roomId"], message["name"]);
}

void Window::HandleRemoveUser(const Json& message)
{
	RemoveUser(message["name"]);
}

void Window::HandeChangeUser(const Json& message)
{
	std::string name = message["name"];
	int ind = FindUser(name);
	if (ind != -1)
	{
		std::string change = message["change"];
		if (change == "roomId")
		{
			ChangeUserRoomId(ind, message["roomId"]);
		}
	}
}

void Window::HandleRoomList(const Json& message)
{
	std::vector<std::string> ids = message["ids"];
	std::vector<std::string> hosts = message["hosts"];
	std::vector<std::string> guests = message["guests"];
	std::vector<bool> locks = message["locks"];
	for (size_t i = 0; i < ids.size(); ++i)
	{
		AddRoom(ids[i], hosts[i], guests[i], locks[i]);
	}
}

void Window::HandleAddRoom(const Json& message)
{
	AddRoom(message["id"], message["host"], message["guest"], message["locked"]);
}

void Window::HandleRemoveRoom(const Json& message)
{
	RemoveRoom(message["id"]);
}

void Window::HandleRoomChange(const Json& message)
{
	std::string id = message["roomId"];
	int ind = FindRoom(id);
	if (ind != -1)
	{
		std::string change = message["change"];
		if (change == "host")
		{
			ChangeRoomHost(ind, message["host"]);
			if (std::atoi(id.c_str()) == roomId && !isHost)
			{
				std::string text = "host: " + std::string(message["host"]);
				SetWindowText(firstPlayerText, text.c_str());
				text = "guest:";
				SetWindowText(secondPlayerText, text.c_str());
				isHost = true;
				SetRoomControl(true);
			}
		}
		else if (change == "guest")
		{
			SetRoomGuest(ind, message["guest"]);
			if (std::atoi(id.c_str()) == roomId)
			{
				std::string text = "guest: " + std::string(message["guest"]);
				SetWindowText(secondPlayerText, text.c_str());
			}
		}
		else if (change == "lock")
		{
			SetRoomLock(ind, message["lock"]);
			if (std::atoi(id.c_str()) == roomId)
			{
				SetRoomLock(message["lock"]);
			}
		}
	}
}

void Window::HandleJoin(const Json& message)
{
	SetConnectionControlsVisibilty(false);
	SetUsersRoomsControlsVisibilty(false);
	roomId = message["roomId"];
	SetRoomControl(message["as"] == "host");
	SetRoomDifficulty(message["difficulty"]);
	SetRoomLock(message["locked"]);
	std::string text = "host: " + std::string(message["host"]);
	SetWindowText(firstPlayerText, text.c_str());
	if (message["guest"] != "")
	{
		std::string text = "guest: " + std::string(message["guest"]);
		SetWindowText(secondPlayerText, text.c_str());
	}
	SetRoomControlsVisibility(true);
}

void Window::HandleQuit(const Json& message)
{
	roomId = 0;
	SetRoomControlsVisibility(false);
	SetConnectionControlsVisibilty(true);
	SetUsersRoomsControlsVisibilty(true);
}

Window::~Window()
{
	DestroyConnectionControls();
	DestroyUsersRoomsControls();
	DestroyRoomControls();
	DestroyWindow(hWnd);
}

LRESULT Window::SetupWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_NCCREATE)
	{
		const CREATESTRUCT* const pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		Window* const pWnd = static_cast<Window*>(pCreate->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PreWndProc));
		return pWnd->WndProc(hWnd, msg, wParam, lParam);
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT Window::PreWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Window* pWnd = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	return pWnd->WndProc(hWnd, msg, wParam, lParam);
}

LRESULT Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
	{
		POINT cursorPos;
		GetCursorPos(&cursorPos);
		ScreenToClient(hWnd, &cursorPos);
		break;
	}
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_KILLFOCUS:
		break;
	/********** KEYBOARD MESSAGES **********/
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
	case WM_CHAR:
	case WM_SYSCHAR:
	/********** END KEYBOARD MESSAGES **********/

	/********** MOUSE MESSAGES **********/
	case WM_LBUTTONDOWN:
		break;
	case WM_LBUTTONUP:
		break;
	case WM_MBUTTONDOWN:
		break;
	case WM_MBUTTONUP:
		break;
	case WM_RBUTTONDOWN:
		break;
	case WM_RBUTTONUP:
		break;
	case WM_MOUSEMOVE:
	{
		const POINTS pt = MAKEPOINTS(lParam);
		break;
	}
	case WM_MOUSEWHEEL:
		break;
	case WM_COMMAND:
		if ((HWND)lParam == connectButton)
		{
			if (client.isConnected())
			{
				client.disconnect();
			}
			else
			{
				std::string name = GetText(nameEdit);
				std::string ip = GetText(ipAddressEdit);
				std::string port = GetText(portEdit);
				client.connect(name, ip, port);
			}
		}
		else if ((HWND)lParam == createButton)
		{
			Json message;
			message["type"] = "createRoom";
			client.sendMessage(message);
		}
		else if ((HWND)lParam == joinButton)
		{
			Json message;
			message["type"] = "join";
			message["roomId"] = selectedRoomId;
			client.sendMessage(message);
		}
		else if ((HWND)lParam == lockButton)
		{
			if (isHost)
			{
				Json message;
				message["type"] = "lock";
				message["roomId"] = roomId;
				message["lock"] = !roomLocked;
				client.sendMessage(message);
			}
		}
		else if ((HWND)lParam == quitButton)
		{
			Json message;
			message["type"] = "quit";
			client.sendMessage(message);
		}
		else if ((HWND)lParam == difficultyCombobox)
		{

		}
		break;
	case WM_NOTIFY:
	{
		LPNMHDR lpm = (LPNMHDR)lParam;
		if (lpm->code == LVN_ITEMACTIVATE)
		{
			int ind = LPNMLISTVIEW(lParam)->iItem;
			selectedRoomId = GetRoomId(ind);
			if (IsRoomJoinable(ind))
			{
				Json message;
				message["type"] = "join";
				message["roomId"] = GetRoomId(ind);
				client.sendMessage(message);
			}
		}
		else if (lpm->code == LVN_ITEMCHANGED)
		{
			int ind = LPNMLISTVIEW(lParam)->iItem;
			selectedRoomId = GetRoomId(ind);
			if (IsRoomJoinable(ind))
			{
				EnableWindow(joinButton, true);
			}
		}
		break;
	}
	case WM_CTLCOLORSTATIC:
	{
		HDC hdc = (HDC)wParam;
		SetBkColor(hdc, RGB(255, 255, 255));
		return (LRESULT)GetStockObject(NULL);
	}
	case WM_CTLCOLORBTN:
	{
		HDC hdc = (HDC)wParam;
		SetBkColor(hdc, RGB(255, 255, 255));
		return (LRESULT)GetStockObject(NULL);
	}
	case WM_SIZE:
	{
		RECT rect;
		GetClientRect(hWnd, &rect);
		width = rect.right;
		height = rect.bottom;
		if (connectionControlsVisible)
			SetConnectionControlsLayout();
		if (roomUsersControlsVisible)
			SetUsersRoomsControlsLayout();
		if (roomControlsVisible)
			SetRoomControlsLayout();
		InvalidateRect(hWnd, NULL, true);
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		FillRect(hdc, &ps.rcPaint, WHITE_BRUSH);
		EndPaint(hWnd, &ps);
		return 0;
	}
	case WM_GETMINMAXINFO:
	{
		LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
		lpmmi->ptMinTrackSize.x = 800;
		lpmmi->ptMinTrackSize.y = 600;
	}
	}
	/********** END MOUSE MESSAGES **********/
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void Window::CreateConnectionControls()
{
	nameText = CreateText("name");
	nameEdit = CreateEdit("Tomek");
	ipAddressText = CreateText("ip address");
	ipAddressEdit = CreateEdit("127.0.0.1");
	portText = CreateText("port");
	portEdit = CreateEdit("1000");
	connectButton = CreateButton("connect");

	SetConnectionControlsLayout();
}

void Window::SetConnectionControlsLayout()
{
	RECT rect;
	GetClientRect(hWnd, &rect);
	int width = (rect.right-50)/4;
	
	// name text
	SetWindowPos(nameText, NULL, 10, 10, width, 20, 0);

	// name edit
	SetWindowPos(nameEdit, NULL, 10, 30, width, 20, 0);

	// ip address text
	SetWindowPos(ipAddressText, NULL, 20+width, 10, width, 20, 0);

	// ip address edit
	SetWindowPos(ipAddressEdit, NULL, 20+width, 30, width, 20, 0);

	// port text
	SetWindowPos(portText, NULL, 30+2*width, 10, width, 20, 0);

	// port edit
	SetWindowPos(portEdit, NULL, 30+2*width, 30, width, 20, 0);

	// connect button
	SetWindowPos(connectButton, NULL, 40+3*width, 10, width, 40, 0);
}

void Window::DestroyConnectionControls()
{
	DestroyWindow(nameText);
	DestroyWindow(nameEdit);
	DestroyWindow(ipAddressText);
	DestroyWindow(ipAddressEdit);
	DestroyWindow(portText);
	DestroyWindow(portEdit);
	DestroyWindow(connectButton);
}

void Window::CreateUsersRoomsControls()
{
	INITCOMMONCONTROLSEX icex;
	icex.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);

	createButton = CreateButton("create");
	joinButton = CreateButton("join");
	roomsText = CreateText("rooms");
	roomsList = CreateRoomsList();
	usersText = CreateText("users");
	usersList = CreateUsersList();
	
	SetUsersRoomsControlsLayout();
}

void Window::SetUsersRoomsControlsLayout()
{
	RECT rect;
	GetClientRect(hWnd, &rect);
	int roomsListWidth = ((rect.right-50)/4)*3+20;
	int usersListWidth = rect.right-30-roomsListWidth;
	int height = rect.bottom-100;

	// create button
	SetWindowPos(createButton, NULL, 10, 60, roomsListWidth/5, 20, 0);

	// join button
	SetWindowPos(joinButton, NULL, 10+roomsListWidth-roomsListWidth/5, 60, roomsListWidth/5, 20, 0);

	// rooms text
	SetWindowPos(roomsText, NULL, 10+roomsListWidth/2-21, 60, 42, 20, 0);
	
	// rooms list
	SetWindowPos(roomsList, NULL, 10, 90, roomsListWidth, height, 0);
	ListView_SetColumnWidth(roomsList, 0, 0);
	ListView_SetColumnWidth(roomsList, 1, 50);
	ListView_SetColumnWidth(roomsList, 2, (roomsListWidth - 108) / 2);
	ListView_SetColumnWidth(roomsList, 3, (roomsListWidth - 108) / 2);
	ListView_SetColumnWidth(roomsList, 4, 20);
	ListView_SetColumnWidth(roomsList, 5, 20);

	// users text
	SetWindowPos(usersText, NULL, roomsListWidth+20+usersListWidth/2-19, 60, 38, 20, 0);

	// users list
	SetWindowPos(usersList, NULL, roomsListWidth+20, 90, usersListWidth, height, 0);
	ListView_SetColumnWidth(usersList, 0, usersListWidth - 70);
	ListView_SetColumnWidth(usersList, 1, 50);
}

void Window::DestroyUsersRoomsControls()
{
	DestroyWindow(createButton);
	DestroyWindow(joinButton);
	DestroyWindow(roomsText);
	DestroyWindow(roomsList);
	DestroyWindow(usersText);
	DestroyWindow(usersList);
}

void Window::AddUser(const std::string& roomId, std::string name)
{
	LVITEM lvi = {};
	lvi.mask = LVIF_TEXT;
	lvi.pszText = (LPSTR)name.c_str();
	lvi.iItem = 0;
	ListView_InsertItem(usersList, &lvi);
	lvi.pszText = (LPSTR)(roomId == "0" ? "-" : roomId.c_str());
	lvi.iSubItem = 1;
	ListView_SetItem(usersList, &lvi);
}

void Window::RemoveUser(const std::string& name)
{
	int ind = FindUser(name);
	if (ind != -1)
	{
		ListView_DeleteItem(usersList, ind);
	}
}

void Window::RemoveAllUsers()
{
	ListView_DeleteAllItems(usersList);
}

void Window::ChangeUserRoomId(int ind, const std::string& newRoomId)
{
	LVITEM lvi = {};
	lvi.mask = LVIF_TEXT;
	lvi.iItem = ind;
	lvi.pszText = (LPSTR)(newRoomId == "0" ? "-" : newRoomId.c_str());
	lvi.iSubItem = 1;
	ListView_SetItem(usersList, &lvi);
}

void Window::AddRoom(const std::string& id, const std::string& host, const std::string& guest, bool locked)
{
	LVITEM lvi = {};
	lvi.mask = LVIF_TEXT;
	lvi.pszText = (LPSTR)id.c_str();
	lvi.iItem = 0;
	ListView_InsertItem(roomsList, &lvi);
	lvi.pszText = (LPSTR)id.c_str();
	lvi.iSubItem = 1;
	ListView_SetItem(roomsList, &lvi);
	lvi.pszText = (LPSTR)host.c_str();
	lvi.iSubItem = 2;
	ListView_SetItem(roomsList, &lvi);
	lvi.pszText = (LPSTR)guest.c_str();
	lvi.iSubItem = 3;
	ListView_SetItem(roomsList, &lvi);
	lvi.mask = LVIF_IMAGE;
	lvi.iImage = (locked || !guest.empty()) ? 2 : 1;
	lvi.iSubItem = 5;
	ListView_SetItem(roomsList, &lvi);
	if (locked)
	{
		lvi.iImage = 0;
		lvi.iSubItem = 4;
		ListView_SetItem(roomsList, &lvi);
	}
	if (ListView_GetItemCount(roomsList) == serverConfig["MAX_NUMBER_OF_ROOMS"])
	{
		EnableWindow(createButton, false);
	}
}

void Window::RemoveRoom(const std::string& id)
{
	int ind = FindRoom(id);
	if (ind != -1)
	{
		ListView_DeleteItem(roomsList, ind);
		EnableWindow(createButton, true);
	}
}

void Window::ChangeRoomHost(int ind, const std::string& newHost)
{
	LVITEM lvi = {};
	lvi.mask = LVIF_TEXT;
	lvi.iItem = ind;
	lvi.pszText = (LPSTR)newHost.c_str();
	lvi.iSubItem = 2;
	ListView_SetItem(roomsList, &lvi);
	lvi.pszText = NULL;
	lvi.iSubItem = 3;
	ListView_SetItem(roomsList, &lvi);
	lvi.mask = LVIF_IMAGE;
	lvi.iItem = ind;
	lvi.iImage = IsRoomJoinable(ind) ? 2 : 1;
	lvi.iSubItem = 5;
	ListView_SetItem(roomsList, &lvi);
}

void Window::SetRoomGuest(int ind, const std::string & guest)
{
	LVITEM lvi = {};
	lvi.mask = LVIF_TEXT;
	lvi.iItem = ind;
	lvi.pszText = (LPSTR)guest.c_str();
	lvi.iSubItem = 3;
	ListView_SetItem(roomsList, &lvi);
	lvi.mask = LVIF_IMAGE;
	lvi.iItem = ind;
	lvi.iImage = IsRoomJoinable(ind) ? 2 : 1;
	lvi.iSubItem = 5;
	ListView_SetItem(roomsList, &lvi);
}

void Window::RemoveAllRooms()
{
	ListView_DeleteAllItems(roomsList);
}

int Window::FindUser(const std::string& name) const
{
	LVFINDINFOA lvi = {};
	lvi.flags = LVFI_STRING;
	lvi.psz = name.c_str();
	return ListView_FindItem(usersList, -1, &lvi);
}

int Window::FindRoom(const std::string& id) const
{
	LVFINDINFOA lvi = {};
	lvi.flags = LVFI_STRING;
	lvi.psz = id.c_str();
	return ListView_FindItem(roomsList, -1, &lvi);
}

bool Window::IsRoomJoinable(int ind) const
{
	LV_ITEM lvi = {};
	lvi.mask = LVIF_IMAGE;
	lvi.iSubItem = 5;
	ListView_GetItem(roomsList, &lvi);
	return lvi.iImage == 1;
}

int Window::GetRoomId(int ind) const
{
	std::string roomId;
	roomId.resize(10);
	LV_ITEM lvi = {};
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 0;
	lvi.pszText = (LPSTR)roomId.c_str();
	lvi.cchTextMax = roomId.size();
	ListView_GetItem(roomsList, &lvi);
	return std::atoi(roomId.c_str());
}

void Window::SetRoomLock(int ind, bool locked)
{
	std::string guest;
	guest.resize(serverConfig["MAX_USER_NAME"]);
	LV_ITEM lvi = {};
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 3;
	lvi.pszText = (LPSTR)guest.c_str();
	lvi.cchTextMax = guest.size();
	ListView_GetItem(roomsList, &lvi);

	lvi.mask = LVIF_IMAGE;
	lvi.iItem = ind;
	lvi.iImage = (locked || guest[0] != '\0') ? 2 : 1;
	lvi.iSubItem = 5;
	ListView_SetItem(roomsList, &lvi);
	lvi.iImage = locked ? 0 : -1;
	lvi.iSubItem = 4;
	ListView_SetItem(roomsList, &lvi);
}

void Window::CreateRoomControls()
{
	sudoku = CreateSudoku();
	quitButton = CreateButton("quit");
	lockBitmap = (HBITMAP)LoadImage(NULL, "icons\\lock.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	notLockBitmap = (HBITMAP)LoadImage(NULL, "icons\\notlock.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	lockButton = CreateImageButton();
	difficultyText = CreateText("difficulty");
	difficultyCombobox = CreateCombobox({ "easy", "medium", "hard", "extreme" });
	firstPlayerText = CreateText("host:");
	firstPlayerPoints = CreateText("points:");
	firstPlayerLifes = CreateText("lifes:");
	secondPlayerText = CreateText("host:");
	secondPlayerPoints = CreateText("points:");
	secondPlayerLifes = CreateText("lifes:");
	kickButton = CreateButton("kick");
	readyButton = CreateButton("READY");
	SetRoomControlsLayout();
}

void Window::SetRoomControlsLayout()
{	
	const int sudokuSize = min(width-20-220, height-20);
	const int sudokuX = (width-sudokuSize-220)/2;
	SetWindowPos(sudoku, NULL, sudokuX, 10, sudokuSize, sudokuSize, 0);
	SetWindowPos(lockButton, NULL, sudokuX+sudokuSize+90, 10, 20, 20, 0);
	SetWindowPos(quitButton, NULL, sudokuX+sudokuSize+120, 10, 100, 20, 0);
	SetWindowPos(difficultyText, NULL, sudokuX+sudokuSize+10, 40, 210, 20, 0);
	SetWindowPos(difficultyCombobox, NULL, sudokuX+sudokuSize+10, 70, 210, 100, 0);
	SetWindowPos(firstPlayerText, NULL, sudokuX+sudokuSize+10, 120, 210, 20, 0);
	SetWindowPos(firstPlayerPoints, NULL, sudokuX+sudokuSize+10, 150, 210, 20, 0);
	SetWindowPos(firstPlayerLifes, NULL, sudokuX+sudokuSize+10, 180, 210, 20, 0);
	SetWindowPos(secondPlayerText, NULL, sudokuX+sudokuSize+10, 230, 210, 20, 0);
	SetWindowPos(secondPlayerPoints, NULL, sudokuX+sudokuSize+10, 260, 210, 20, 0);
	SetWindowPos(secondPlayerLifes, NULL, sudokuX+sudokuSize+10, 290, 210, 20, 0);
	SetWindowPos(kickButton, NULL, sudokuX+sudokuSize+10, 320, 210, 20, 0);
	SetWindowPos(readyButton, NULL, sudokuX + sudokuSize + 10, 10+sudokuSize-50, 210, 50, 0);
}

void Window::SetRoomControlsVisibility(bool visible)
{
	roomControlsVisible = visible;
	int command = visible ? SW_SHOW : SW_HIDE;
	ShowWindow(sudoku, command);
	ShowWindow(quitButton, command);
	ShowWindow(lockButton, command);
	ShowWindow(difficultyText, command);
	ShowWindow(difficultyCombobox, command);
	ShowWindow(firstPlayerText, command);
	ShowWindow(firstPlayerPoints, command);
	ShowWindow(firstPlayerLifes, command);
	ShowWindow(secondPlayerText, command);
	ShowWindow(secondPlayerPoints, command);
	ShowWindow(secondPlayerLifes, command);
	ShowWindow(kickButton, command);
	ShowWindow(readyButton, command);
	SetRoomControlsLayout();
	InvalidateRect(hWnd, NULL, true);
}

void Window::DestroyRoomControls()
{
	DestroyWindow(sudoku);
	DestroyWindow(quitButton);
	DestroyWindow(lockButton);
	DeleteObject(lockBitmap);
	DeleteObject(notLockBitmap);
	DestroyWindow(difficultyText);
	DestroyWindow(difficultyCombobox);
	DestroyWindow(firstPlayerText);
	DestroyWindow(firstPlayerPoints);
	DestroyWindow(firstPlayerLifes);
	DestroyWindow(secondPlayerText);
	DestroyWindow(secondPlayerPoints);
	DestroyWindow(secondPlayerLifes);
	DestroyWindow(kickButton);
	DestroyWindow(readyButton);
}

void Window::SetRoomControl(bool hasControl)
{
	isHost = hasControl;
	EnableWindow(difficultyCombobox, hasControl);
	EnableWindow(kickButton, hasControl);
}

void Window::SetRoomLock(bool locked)
{
	SendMessage(lockButton, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(locked ? lockBitmap : notLockBitmap));
	roomLocked = locked;
}

void Window::SetRoomDifficulty(int difficulty)
{
	ComboBox_SetCurSel(difficultyCombobox, difficulty);
}

HWND Window::CreateText(const std::string& text)
{
	HWND hText = CreateWindow(
		"STATIC",
		text.c_str(),
		WS_CHILD,
		0, 0, 0, 0,
		hWnd,
		NULL,
		WindowClass::GetInstance(),
		NULL
	);
	if (hText == NULL)
		throw WND_LAST_EXCEPTION;

	return hText;
}

HWND Window::CreateEdit(const std::string& text)
{
	HWND edit = CreateWindow(
		"EDIT",
		text.c_str(),
		WS_CHILD | WS_BORDER,
		0, 0, 0, 0,
		hWnd,
		NULL,
		WindowClass::GetInstance(),
		NULL
	);
	if (edit == NULL)
		throw WND_LAST_EXCEPTION;

	return edit;
}

HWND Window::CreateButton(const std::string& text)
{
	HWND button = CreateWindow(
		"BUTTON",
		text.c_str(),
		WS_CHILD | WS_BORDER | BS_DEFPUSHBUTTON,
		0, 0, 0, 0,
		hWnd,
		NULL,
		WindowClass::GetInstance(),
		NULL
	);
	if (button == NULL)
		throw WND_LAST_EXCEPTION;
	
	return button;
}

HWND Window::CreateImageButton()
{
	HWND button = CreateWindow(
		"BUTTON",
		"",
		WS_CHILD | BS_FLAT | BS_BITMAP,
		0, 0, 0, 0,
		hWnd,
		NULL,
		WindowClass::GetInstance(),
		NULL
	);
	if (button == NULL)
		throw WND_LAST_EXCEPTION;
	
	return button;
}

HWND Window::CreateCombobox(const std::vector<std::string>& items)
{
	HWND combobox = CreateWindow(
		"COMBOBOX",
		"",
		WS_CHILD | WS_BORDER | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
		0, 0, 0, 0,
		hWnd,
		NULL,
		WindowClass::GetInstance(),
		NULL
	);
	if (combobox == NULL)
		throw WND_LAST_EXCEPTION;

	for (const std::string& item : items)
	{
		SendMessage(combobox, CB_ADDSTRING, (WPARAM)0, (LPARAM)item.c_str());
	}
}

HWND Window::CreateRoomsList()
{
	HWND roomsList = CreateWindow(
		WC_LISTVIEW,
		"",
		WS_CHILD | WS_BORDER | LVS_REPORT,
		0, 0, 0, 0,
		hWnd,
		NULL,
		WindowClass::GetInstance(),
		NULL
	);
	if (roomsList == NULL)
		throw WND_LAST_EXCEPTION;

	ListView_SetExtendedListViewStyle(roomsList, LVS_EX_GRIDLINES | LVS_EX_SUBITEMIMAGES | LVS_EX_SINGLEROW | LVS_EX_SIMPLESELECT | LVS_EX_FULLROWSELECT);
	HIMAGELIST hil = ImageList_Create(20, 20, ILC_COLORDDB, 3, 0);
	HBITMAP hb = (HBITMAP)LoadImage(NULL, "icons\\lock.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	ImageList_Add(hil, hb, NULL);
	DeleteObject(hb);
	hb = (HBITMAP)LoadImage(NULL, "icons\\canjoin.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	ImageList_Add(hil, hb, NULL);
	DeleteObject(hb);
	hb = (HBITMAP)LoadImage(NULL, "icons\\cantjoin.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	ImageList_Add(hil, hb, NULL);
	DeleteObject(hb);

	ListView_SetGroupHeaderImageList(roomsList, hil);
	ListView_SetImageList(roomsList, hil, LVSIL_SMALL);

	LVCOLUMN lvc = {};
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.fmt = LVCFMT_LEFT | LVCFMT_FIXED_WIDTH;
	lvc.iSubItem = 0;
	lvc.cx = 0;
	lvc.pszText = (LPSTR)"id";
	ListView_InsertColumn(roomsList, 0, &lvc);
	ListView_InsertColumn(roomsList, 1, &lvc);
	lvc.iSubItem = 1;
	lvc.pszText = (LPSTR)"host";
	ListView_InsertColumn(roomsList, 2, &lvc);
	lvc.iSubItem = 2;
	lvc.pszText = (LPSTR)"guest";
	ListView_InsertColumn(roomsList, 3, &lvc);
	lvc.mask = LVCF_WIDTH;
	lvc.iSubItem = 3;
	ListView_InsertColumn(roomsList, 4, &lvc);
	lvc.iSubItem = 4;
	ListView_InsertColumn(roomsList, 5, &lvc);

	return roomsList;
}

HWND Window::CreateUsersList()
{
	HWND usersList = CreateWindow(
		WC_LISTVIEW,
		"",
		WS_CHILD | WS_BORDER | LVS_REPORT,
		0, 0, 0, 0,
		hWnd,
		NULL,
		WindowClass::GetInstance(),
		NULL
	);
	if (usersList == NULL)
		throw WND_LAST_EXCEPTION;

	ListView_SetExtendedListViewStyle(usersList, LVS_EX_GRIDLINES | LVS_EX_SIMPLESELECT);

	LVCOLUMN lvc = {};
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.iSubItem = 0;
	lvc.pszText = (LPSTR)"name";
	ListView_InsertColumn(usersList, 0, &lvc);
	lvc.iSubItem = 1;
	lvc.pszText = (LPSTR)"room";
	ListView_InsertColumn(usersList, 1, &lvc);

	return usersList;
}

HWND Window::CreateSudoku()
{
	HWND sudoku = CreateWindow(
		"STATIC",
		"",
		WS_CHILD | WS_BORDER | BS_DEFPUSHBUTTON,
		0, 0, 0, 0,
		hWnd,
		NULL,
		WindowClass::GetInstance(),
		NULL
	);
	if (sudoku == NULL)
		throw WND_LAST_EXCEPTION;

	return sudoku;
}

Window::HrException::HrException(int line, const char * file, HRESULT hResult)
	:
	Exception(line, file),
	hResult(hResult)
{}

const char * Window::HrException::what() const
{
	std::ostringstream oss;
	oss << GetType() << std::endl
		<< "[Error code] " << GetErrorCode() << std::endl
		<< "[Description] " << GetErrorString() << std::endl
		<< GetOriginString();
	whatBuffer = oss.str();
	return whatBuffer.c_str();
}

const char * Window::HrException::GetType() const
{
	return "Window exception";
}

HRESULT Window::HrException::GetErrorCode() const
{
	return hResult;
}

std::string Window::Exception::TranslateErrorCode(HRESULT hResult)
{
	char* pMsgBuff = NULL;
	DWORD nMsgLen = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		hResult,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPSTR>(&pMsgBuff),
		0,
		NULL
	);
	if (nMsgLen == 0)
		return "Undefined error code";
	std::string errorString = pMsgBuff;
	LocalFree(pMsgBuff);
	return errorString;
}

std::string Window::HrException::GetErrorString() const
{
	return Window::Exception::TranslateErrorCode(hResult);
}

std::string GetText(HWND hWnd)
{
	int size = GetWindowTextLength(hWnd);
	char* text = new char[size+1];
	GetWindowText(hWnd, (LPSTR)text, size+1);
	return text;
}