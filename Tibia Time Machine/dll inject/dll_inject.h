#pragma once
#include "main.h"

HMODULE HookWindow(CONST HANDLE Process, CONST HWND Wnd, CONST LPCSTR DLLName, CONST LPCSTR FuncName);
VOID UnhookWindow(CONST HANDLE Process, CONST HWND Wnd, CONST HMODULE Module, CONST LPCSTR FuncName);