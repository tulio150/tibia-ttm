#include "dll_inject.h"

struct WORKSPACE {
	DWORD Index;
	BYTE Data[1024];

	WORKSPACE(): Index(0) {}

	VOID Write(CONST LPCVOID Src, CONST DWORD Size) {
		CopyMemory(Data + Index, Src, Size);
		Index += Size;
	}
	VOID String(CONST LPCSTR Src) {
		Write(Src, strlen(Src) + 1);
	}
	VOID Byte(CONST BYTE Src) {
		Data[Index++] = Src;
	}
	VOID Pointer(CONST LPCVOID Src) {
		*(LPCVOID *)(Data + Index) = Src;
		Index += 4;
	}
};

HMODULE HookWindow(CONST HANDLE Process, CONST HWND Wnd, CONST LPCSTR DLLName, CONST LPCSTR FuncName) {
	if (!PathFileExists(DLLName)) {
		return NULL;
	}
	LPBYTE Address = (LPBYTE) VirtualAllocEx(Process, 0, 1024, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!Address) {
		return NULL;
	}

	WORKSPACE Workspace;

	LPBYTE DLLNameAddr = Address + Workspace.Index;
	Workspace.String(DLLName);

	LPBYTE Module = Address + Workspace.Index;
	Workspace.Pointer(NULL);

	LPBYTE FuncNameAddr = Address + Workspace.Index;
	Workspace.String(FuncName);

	LPBYTE ExecAddr = Address + Workspace.Index;

	{// eax = LoadLibrary(DLLNameAddr)
		// PUSH ADDRESS - Push the address of the DLL name
		Workspace.Byte(0x68);
		Workspace.Pointer(DLLNameAddr);

		// MOV EAX, ADDRESS - Move the address of LoadLibraryA into EAX
		Workspace.Byte(0xB8);
		Workspace.Pointer(LoadLibrary);

		// CALL EAX - Call LoadLibraryA
		Workspace.Byte(0xFF);
		Workspace.Byte(0xD0);
	}

	{// if (!eax) goto return
		// CMP EAX, 0 - Compare with zero
		Workspace.Byte(0x83);
		Workspace.Byte(0xF8);
		Workspace.Byte(0x00);

		// JZ EIP + SIZE - Skip over code if zero
		Workspace.Byte(0x74);
		Workspace.Byte(35);
	}

	{// *Module = eax
		// MOV [ADDRESS], EAX - Save the module handle
		Workspace.Byte(0xA3);
		Workspace.Pointer(Module);
	}

	{// eax = GetProcAddress(eax, FuncNameAddr);
		// PUSH ADDRESS - Push the address of the function name
		Workspace.Byte(0x68);
		Workspace.Pointer(FuncNameAddr);

		// PUSH EAX - Push module handle
		Workspace.Byte(0x50);

		// MOV EAX, ADDRESS - Move the address of GetProcAddress into EAX
		Workspace.Byte(0xB8);
		Workspace.Pointer(GetProcAddress);

		// CALL EAX - Call GetProcAddress
		Workspace.Byte(0xFF);
		Workspace.Byte(0xD0);
	}

	{// if (!eax) goto error
		// CMP EAX, 0 - Compare with zero
		Workspace.Byte(0x83);
		Workspace.Byte(0xF8);
		Workspace.Byte(0x00);

		// JZ EIP + SIZE - Skip over code if zero
		Workspace.Byte(0x74);
		Workspace.Byte(25);
	}

	{// eax = eax(Wnd)
		// PUSH ADDRESS - Push the window handle
		Workspace.Byte(0x68);
		Workspace.Pointer(Wnd);

		// CALL EAX - Call Initialize function
		Workspace.Byte(0xFF);
		Workspace.Byte(0xD0);
	}

	{// if (!eax) goto error
		// CMP EAX, 0 - Compare with zero
		Workspace.Byte(0x83);
		Workspace.Byte(0xF8);
		Workspace.Byte(0x00);

		// JZ EIP + size - Skip over code
		Workspace.Byte(0x74);
		Workspace.Byte(13);
	}

	{// :return ExitThread(*Module)
		// PUSH [ADDRESS] - Push the return code
		Workspace.Byte(0xFF);
		Workspace.Byte(0x35);
		Workspace.Pointer(Module);

		// MOV EAX, ADDRESS - Move the address of ExitThread into EAX
		Workspace.Byte(0xB8);
		Workspace.Pointer(ExitThread);

		// CALL EAX - Call ExitThread
		Workspace.Byte(0xFF);
		Workspace.Byte(0xD0);
	}

	{// :error FreeLibraryAndExitThread(*Module, NULL)
		// PUSH NULL - Push the return code
		Workspace.Byte(0x6A);
		Workspace.Byte(NULL);

		// PUSH [ADDRESS] - Push the module handle
		Workspace.Byte(0xFF);
		Workspace.Byte(0x35);
		Workspace.Pointer(Module);

		// MOV EAX, ADDRESS - Move the address of FreeLibraryAndExitThread into EAX
		Workspace.Byte(0xB8);
		Workspace.Pointer(FreeLibraryAndExitThread);

		// CALL EAX - Call FreeLibraryAndExitThread
		Workspace.Byte(0xFF);
		Workspace.Byte(0xD0);
	}

	HMODULE RemoteModule = NULL;
	if (WriteProcessMemory(Process, Address, Workspace.Data, Workspace.Index, NULL)) {
		FlushInstructionCache(Process, Address, Workspace.Index);

		HANDLE Thread = CreateRemoteThread(Process, NULL, 0, (LPTHREAD_START_ROUTINE) ExecAddr, 0, 0, NULL);
		if (Thread) {
			WaitForSingleObject(Thread, INFINITE);
			GetExitCodeThread(Thread, (LPDWORD) &RemoteModule);
			CloseHandle(Thread);
		}
	}

	VirtualFreeEx(Process, Address, 0, MEM_RELEASE);
	return RemoteModule;
}

VOID UnhookWindow(CONST HANDLE Process, CONST HWND Wnd, CONST HMODULE Module, CONST LPCSTR FuncName) {
	if (!Module) {
		return;
	}
	LPBYTE Address = (LPBYTE) VirtualAllocEx(Process, 0, 1024, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!Address) {
		return;
	}

	WORKSPACE Workspace;

	LPBYTE FuncNameAddr = Address + Workspace.Index;
	Workspace.String(FuncName);

	LPBYTE ExecAddr = Address + Workspace.Index;

	{// eax = GetProcAddress(Module, FuncNameAddr);
		// PUSH ADDRESS - Push the address of the function name
		Workspace.Byte(0x68);
		Workspace.Pointer(FuncNameAddr);

		// PUSH ADDRESS - Push the module handle
		Workspace.Byte(0x68);
		Workspace.Pointer(Module);

		// MOV EAX, ADDRESS - Move the address of GetProcAddress into EAX
		Workspace.Byte(0xB8);
		Workspace.Pointer(GetProcAddress);

		// CALL EAX - Call GetProcAddress
		Workspace.Byte(0xFF);
		Workspace.Byte(0xD0);
	}

	{// if (!eax) goto return
		// CMP EAX, 0 - Compare with zero
		Workspace.Byte(0x83);
		Workspace.Byte(0xF8);
		Workspace.Byte(0x00);

		// JZ EIP + size - Skip over code
		Workspace.Byte(0x74);
		Workspace.Byte(7);
	}

	{// eax(Wnd)
		// PUSH ADDRESS - Push the window handle
		Workspace.Byte(0x68);
		Workspace.Pointer(Wnd);

		// CALL EAX - Call Cleanup function
		Workspace.Byte(0xFF);
		Workspace.Byte(0xD0);
	}

	{// :return FreeLibraryAndExitThread(Module, NULL)
		// PUSH NULL - Push the return code
		Workspace.Byte(0x6A);
		Workspace.Byte(NULL);

		// PUSH ADDRESS - Push the module handle
		Workspace.Byte(0x68);
		Workspace.Pointer(Module);

		// MOV EAX, ADDRESS - Move the address of FreeLibraryAndExitThread into EAX
		Workspace.Byte(0xB8);
		Workspace.Pointer(FreeLibraryAndExitThread);

		// CALL EAX - Call FreeLibraryAndExitThread
		Workspace.Byte(0xFF);
		Workspace.Byte(0xD0);
	}

	if (WriteProcessMemory(Process, Address, Workspace.Data, Workspace.Index, NULL)) {
		FlushInstructionCache(Process, Address, Workspace.Index);

		HANDLE Thread = CreateRemoteThread(Process, NULL, 0, (LPTHREAD_START_ROUTINE) ExecAddr, 0, 0, NULL);
		if (Thread) {
			WaitForSingleObject(Thread, INFINITE);
			CloseHandle(Thread);
		}
	}

	VirtualFreeEx(Process, Address, 0, MEM_RELEASE);
}