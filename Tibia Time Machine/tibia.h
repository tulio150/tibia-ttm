#pragma once
#include "framework.h"

#define PORT 7171

namespace Tibia {
	extern TCHAR VersionString[6];
	extern WORD Version;
	extern BYTE HostLen;
	extern CHAR Host[128];
	extern WORD Port;
	extern DWORD Signatures[3];
	extern PROCESS_INFORMATION Proc;

	extern HANDLE Running;

	namespace RSA {
		extern CONST DWORD Public;
		extern CONST DWORD Private[32];
		extern CONST DWORD Modulus[32];
		extern DWORD ServerModulus[32];
	}

	class HostLoop {
		HKEY Key;

		VOID ImportLegacy();
		BOOL PromptClientPath(CONST LPTSTR Path);
	public:
		union {
			DWORD History;
			struct {
				WORD Version;
				WORD Port;
			};
		};
		TCHAR Host[0x7FFF];

		BOOL Start();
		BOOL Next(CONST DWORD Index);
		BOOL Delete();
		VOID End() CONST;

		BOOL GetClientPath(CONST LPTSTR Path);
		VOID SaveHistory();
	};

	VOID StartURL(CONST LPTSTR Url);
	VOID Start();
	VOID Flash();
	VOID Close();

	VOID SetVersionString(CONST WORD Version);
	WORD VersionFromString();
	BOOL VerifyHost(CONST LPCSTR Host, CONST BYTE Len);
	BOOL VerifyHost(CONST LPCWSTR Host, CONST BYTE Len);
	VOID SetHost(CONST WORD Version, CONST BYTE HostLen, CONST LPCSTR Host, CONST WORD Port);
	VOID SetHost(CONST WORD Version, CONST BYTE HostLen, CONST LPCWSTR Host, CONST WORD Port);

	VOID AutoPlay();
	VOID AutoPlayCharlist();

	VOID OpenVersionMenu();
	VOID CloseVersionMenu();


	LPCTSTR GetIcon();

	VOID Lock();
	VOID Unlock();
	VOID Redraw();
	VOID CloseDialogs();
}