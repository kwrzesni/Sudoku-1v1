#pragma once
#include "LeanWindows.h"
#include "BaseException.h"
#include "Client.h"
#include <optional>

class Window
{
public:
	class Exception : public BaseException
	{
		using BaseException::BaseException;
	public:
		static std::string TranslateErrorCode(HRESULT hResult);
	};
	class HrException : public Exception
	{
	public:
		HrException(int line, const char* file, HRESULT hResult);
		const char* what() const override;
		const char* GetType() const override;
		HRESULT GetErrorCode() const;
		
		std::string GetErrorString() const;
	private:
		HRESULT hResult;
	};
private:
	class WindowClass
	{
	public:
		static const TCHAR* GetName();
		static HINSTANCE GetInstance();
	private:
		WindowClass();
		WindowClass(const WindowClass&) = delete;
		WindowClass& operator=(const WindowClass&) = delete;
		~WindowClass();
	private:
		static WindowClass wndClass;
		static constexpr const TCHAR* name = TEXT("Sudoku 1v1");
		HINSTANCE hInstance;
	};
public:
	Window(int width, int height, const TCHAR* name, Client& client);
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	void SetTitle(const std::string& title);
	static std::optional<int> ProcesMessage();
	void SetConnectionControlsVisibilty(bool visible);
	void SetUsersRoomsControlsVisibilty(bool visible);
	// handlers
	void HandleConnection();
	void HandleDisconnection();
	void HandleServerConfig(const Json& message);
	void HandleUserList(const Json& message);
	void HandleAddUser(const Json& message);
	void HandleRemoveUser(const Json& message);
	void HandeChangeUser(const Json& message);
	void HandleRoomList(const Json& message);
	void HandleAddRoom(const Json& message);
	void HandleRemoveRoom(const Json& message);
	void HandleRoomChange(const Json& message);
	void HandleJoin(const Json& message);
	void HandleQuit(const Json& message);
	~Window();
private:
	static LRESULT CALLBACK SetupWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK PreWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	// conection controls
	void CreateConnectionControls();
	void SetConnectionControlsLayout();
	void DestroyConnectionControls();
	// rooms/users controls
	void CreateUsersRoomsControls();
	void SetUsersRoomsControlsLayout();
	void DestroyUsersRoomsControls();
	void AddUser(const std::string& roomId, std::string name);
	void RemoveUser(const std::string& name);
	void RemoveAllUsers();
	void ChangeUserRoomId(int ind, const std::string& newRoomId);
	void AddRoom(const std::string& id, const std::string& host, const std::string& guest, bool locked);
	void RemoveRoom(const std::string& id);
	void ChangeRoomHost(int ind, const std::string& newHost);
	void SetRoomGuest(int ind, const std::string& guest);
	void RemoveAllRooms();
	int FindUser(const std::string& name) const;
	int FindRoom(const std::string& id) const;
	bool IsRoomJoinable(int ind) const;
	int GetRoomId(int ind) const;
	void SetRoomLock(int ind, bool locked);
	// room controls
	void CreateRoomControls();
	void SetRoomControlsLayout();
	void SetRoomControlsVisibility(bool visible);
	void DestroyRoomControls();
	void SetRoomControl(bool hasControl);
	void SetRoomLock(bool locked);
	void SetRoomDifficulty(int difficulty);
	// helpers
	HWND CreateText(const std::string& text);
	HWND CreateEdit(const std::string& text);
	HWND CreateButton(const std::string& text);
	HWND CreateImageButton();
	HWND CreateCombobox(const std::vector<std::string> &items);
	HWND CreateRoomsList();
	HWND CreateUsersList();
	HWND CreateSudoku();
private:
	int width;
	int height;
	HWND hWnd;
	// connection controls
	HWND nameText;
	HWND nameEdit;
	HWND ipAddressText;
	HWND ipAddressEdit;
	HWND portText;
	HWND portEdit;
	HWND connectButton;
	bool connectionControlsVisible = true;
	// rooms/users controls
	HWND createButton;
	HWND joinButton;
	HWND roomsText;
	HWND roomsList;
	HWND usersText;
	HWND usersList;
	bool roomUsersControlsVisible = false;
	int selectedRoomId = 0;
	// room controls
	HWND sudoku;
	HWND quitButton;
	HWND lockButton;
	HBITMAP notLockBitmap;
	HBITMAP lockBitmap;
	HWND difficultyText;
	HWND difficultyCombobox;
	HWND firstPlayerText;
	HWND firstPlayerPoints;
	HWND firstPlayerLifes;
	HWND secondPlayerText;
	HWND secondPlayerPoints;
	HWND secondPlayerLifes;
	HWND kickButton;
	HWND readyButton;
	int roomId = 0;
	bool roomLocked = false;
	bool roomControlsVisible = false;
	bool isHost;
	// Client
	Client& client;
	// server config
	Json serverConfig;
};

