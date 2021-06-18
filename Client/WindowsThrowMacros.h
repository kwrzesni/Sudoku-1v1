#pragma once
#define WND_EXCEPTION(hResult) Window::HrException(__LINE__, __FILE__, hResult)
#define WND_NO_GFX_EXCEPTION() Window::NoGfxException(__LINE__, __FILE__)
#define WND_LAST_EXCEPTION Window::HrException(__LINE__, __FILE__, GetLastError())