#include "tibia.h"
#include "main.h"
#include "loader.h"
#include "file.h"
#include "video.h"
#include "proxy.h"

#define SECTION_TEXT 0
#define SECTION_RDATA 1
#define SECTION_DATA 2
#define TIMEOUT 8000 //time to wait for the Tibia window to show-up

namespace Tibia {
	TCHAR VersionString[6] = _T("11.20");
	WORD Version = LATEST;
	BYTE HostLen = NULL;
	CHAR Host[128] = "";
	WORD Port = PORT;
	BOOL CustomClient;
	DWORD Signatures[3];
	PROCESS_INFORMATION Proc;
	HWND Wnd;

	HANDLE Running = NULL;

	namespace RSA {
		CONST DWORD Public = 65537;
		CONST DWORD Private[32] = {0xAEFE8D81, 0xDF443761, 0x6CC337F7, 0xB7B08A05, 0x0C2BFA1F, 0xAF311E85, 0xDB64FF9B, 0xA1E28519, 0xA28A8A09, 0xE786CF81, 0xB75B1825, 0x322A1E2A, 0xA3A0AA21, 0x7FEEE9B4, 0x6D8F4583, 0x5856E7AA, 0x0B782886, 0xAD7B5D44, 0x20CA6DB7, 0x27563312, 0x1CED1FAC, 0xE2EA4E0A, 0x48BAC410, 0x99DE8CE6, 0x52B02613, 0x0BB6378B, 0x549C5466, 0xF8C857D6, 0x71A43102, 0xA761106F, 0x346DAF71, 0x428BD3B5};
		CONST DWORD Modulus[32] = {0x8D5B7FF5, 0xAAB33DBE, 0x777E10AC, 0xF5C401D6, 0x15C1E3CB, 0x528A5D3F, 0xB0DE1930, 0x1B4E15BE, 0x1FD460F0, 0xC24A5BD5, 0x9C321C5, 0x3D759715, 0xC142A75D, 0xCFBEE7AD, 0x2AD3DBB3, 0x2611C615, 0x9B72B121, 0x35F8B105, 0xA58A1A1B, 0x882AB5BC, 0x80594909, 0x9452FA00, 0x561D7CCB, 0xA837AA60, 0x399661B4, 0x3E6DD079, 0x940703B0, 0x65139DD7, 0x7353BD71, 0x956568D8, 0xB45B07AC, 0x9B646903};
		//CONST DWORD GlobalModulus[32] = {0xF84B1949, 0xE9D8AEB7, 0x82F1F66E, 0x23053128, 0x8BDAF869, 0x2C29AA68, 0x72EE60A5, 0x3B3FC7A8, 0xBFAFAF74, 0xCEC2C3E5, 0x96618FFF, 0xA2C788EE, 0x4031F3E4, 0x2C9E1C50, 0xFFFFF18F, 0x8A6F7C2E, 0x4531EFE4, 0x245C1A6C, 0x86797E12, 0x4263FC49, 0x53E9C591, 0x56D62F41, 0x978DD291, 0xD9D9E159, 0xB4FA32F7, 0xBD8634D8, 0x3EE34FB, 0x1CF43D28, 0x1CEF8FD5, 0x43F4DFBE, 0xA96B8E2A, 0xBC27F992};

		CONST CHAR Modulus_761[310] = "142996239624163995200701773828988955507954033454661532174705160829347375827760388829672133862046006741453928458538592179906264509724520840657286865659265687630979195970404721891201847792002125535401292779123937207447574596692788513647179235335529307251350570728407373705564708871762033017096809910315212883967";
		CONST CHAR Modulus_771[310] = "124710459426827943004376449897985582167801707960697037164044904862948569380850421396904597686953877022394604239428185498284169068581802277612081027966724336319448537811441719076484340922854929273517308661370727105382899118999403808045846444647284499123164879035103627004668521005328367415259939915284902061793";
		CONST CHAR Modulus_861[310] = "132127743205872284062295099082293384952776326496165507967876361843343953435544496682053323833394351797728954155097012103928360786959821132214473291575712138800495033169914814069637740318278150290733684032524174782740134357629699062987023311132821016569775488792221429527047321331896351555606801473202394175817";
		CONST CHAR Modulus_OTS[310] = "109120132967399429278860960508995541528237502902798129123468757937266291492576446330739696001110603907230888610072655818825358503429057592827629436413108566029093628212635953836686562675849720620786279431090218017681061521755056710823876476444260558147179707119674283982419152118103759076030616683978566631413";
	}
	
	BOOL VerifyHost(CONST LPCSTR Host, CONST BYTE Len) {
		for (BYTE i = 0; i < Len; i++) {
			if (Host[i] == '-') {
				if (!Host[i+1]) {
					return FALSE;
				}
			}
			else if (Host[i] != '.' && !IsCharDigit(Host[i]) && Host[i] != '_' && NotLowLetter(Host[i])) {
				return FALSE;
			}
		}
		return TRUE;
	}
	BOOL VerifyHost(CONST LPCWSTR Host, CONST BYTE Len) {
		for (BYTE i = 0; i < Len; i++) {
			if (Host[i] == '-') {
				if (!Host[i + 1]) {
					return FALSE;
				}
			}
			else if (Host[i] != '.' && !IsCharDigit(Host[i]) && Host[i] != '_' && NotLowLetter(Host[i])) {
				return FALSE;
			}
		}
		return TRUE;
	}

	VOID SetHost(CONST WORD NewVersion, CONST BYTE NewHostLen, CONST LPCSTR NewHost, CONST WORD NewPort) {
		Version = NewVersion;
		Host[HostLen = NewHostLen] = NULL;
		CopyMemory(Host, NewHost, HostLen);
		Port = NewPort;
	}
	VOID SetHost(CONST WORD NewVersion, CONST BYTE NewHostLen, CONST LPCWSTR NewHost, CONST WORD NewPort) {
		Version = NewVersion;
		Host[HostLen = NewHostLen] = NULL;
		CopyMemoryW(Host, NewHost, HostLen);
		Port = NewPort;
	}

	VOID SetVersionString(CONST WORD Version) {
		if (Version < 1000) {
			ListAssign(3, Split3(VersionString, 0, 2, 3), CharDigits(3, Version));
			ListAssign(2, Split2(VersionString, 1, 4), List2('.', 0));
		}
		else {
			ListAssign(4, Split4(VersionString, 0, 1, 3, 4), CharDigits(4, Version));
			ListAssign(2, Split2(VersionString, 2, 5), List2('.', 0));
		}
	}

	WORD VersionFromString() {
		while (VersionString[0] == '0') {
			ListAssign(5, Split5(VersionString, 0, 1, 2, 3, 4), Split5(VersionString, 1, 2, 3, 4, 5));
			VersionString[5] = 'x';
		}
		if (VersionString[0]) {
			if (!VersionString[1]) { //X -> X.00
				if (IsCharDigit(VersionString[0])) {
					if (VersionString[0] >= '7') {
						ListAssign(4, Split4(VersionString, 1, 2, 3, 4), List4('.', '0', '0', 0));
						return CharToDigit(VersionString[0]) * 100;
					}
				}
			}
			else if (!VersionString[2]) {
				if (VersionString[1] == '.') { //X. -> X.00
					if (IsCharDigit(VersionString[0])) {
						if (VersionString[0] >= '7') {
							ListAssign(3, Split3(VersionString, 2, 3, 4), List3('0', '0', 0));
							return CharToDigit(VersionString[0]) * 100;
						}
					}
				}
				else if (AreCharDigits(2, Split2(VersionString, 0, 1))) { //XX -> X.X0 or XX.00
					if (VersionString[0] >= '7') { //XX -> X.X0
						VersionString[2] = VersionString[1];
						ListAssign(3, Split3(VersionString, 1, 3, 4), List3('.', '0', 0));
						return DecimalChars(2, Split2(VersionString, 0, 2)) * 10;
					}
					ListAssign(4, Split4(VersionString, 2, 3, 4, 5), List4('.', '0', '0', 0));
					return DecimalChars(2, Split2(VersionString, 0, 1)) * 100;
				}
			}
			else if (!VersionString[3]) {
				if (VersionString[1] == '.') { //X.X -> X.X0
					if (AreCharDigits(2, Split2(VersionString, 0, 2))) {
						if (VersionString[0] >= '7') {
							ListAssign(2, Split2(VersionString, 3, 4), List2('0', 0));
							return DecimalChars(2, Split2(VersionString, 0, 2)) * 10;
						}
					}
				}
				else if (VersionString[2] == '.') { //XX. -> XX.00
					if (AreCharDigits(2, Split2(VersionString, 0, 1))) {
						ListAssign(3, Split3(VersionString, 3, 4, 5), List3('0', '0', 0));
						return DecimalChars(2, Split2(VersionString, 0, 1)) * 100;
					}
				}
				else if (AreCharDigits(3, Split3(VersionString, 0, 1, 2))) { //XXX -> X.XX or XX.X0
					if (VersionString[0] >= '7') { //XXX -> X.XX
						ListAssign(2, Split2(VersionString, 3, 2), Split2(VersionString, 2, 1));
						ListAssign(2, Split2(VersionString, 1, 4), List2('.', 0));
						return DecimalChars(3, Split3(VersionString, 0, 2, 3));
					}
					VersionString[3] = VersionString[2];
					ListAssign(3, Split3(VersionString, 2, 4, 5), List3('.', '0', 0));
					return DecimalChars(3, Split3(VersionString, 0, 1, 3)) * 10;
				}
			}
			else if (!VersionString[4]) {
				if (VersionString[1] == '.') { //X.XX
					if (AreCharDigits(3, Split3(VersionString, 0, 2, 3))) {
						if (VersionString[0] >= '7') {
							return DecimalChars(3, Split3(VersionString, 0, 2, 3));
						}
					}
				}
				else if (VersionString[2] == '.') { //XX.X -> XX.X0
					if (AreCharDigits(3, Split3(VersionString, 0, 1, 3))) {
						ListAssign(2, Split2(VersionString, 4, 5), List2('0', 0));
						return DecimalChars(3, Split3(VersionString, 0, 1, 3)) * 10;
					}
				}
				else if (AreCharDigits(4, Split4(VersionString, 0, 1, 2, 3))) { //XXXX -> XX.XX
					ListAssign(2, Split2(VersionString, 4, 3), Split2(VersionString, 3, 2));
					ListAssign(2, Split2(VersionString, 2, 5), List2('.', 0));
					return DecimalChars(4, Split4(VersionString, 0, 1, 3, 4));
				}
			}
			else if (!VersionString[5]) { //XX.XX
				if (VersionString[2] == '.' && AreCharDigits(4, Split4(VersionString, 0, 1, 3, 4))) {
					return DecimalChars(4, Split4(VersionString, 0, 1, 3, 4));
				}
			}
		}
		return 0;
	}

	BOOL HostLoop::Start() {
		return RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\Tibia\\Server"), 0, NULL, NULL, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &Key, NULL) == ERROR_SUCCESS;
	}
	BOOL HostLoop::Next(CONST DWORD Index) {
		for (DWORD HostLen, Type, DataLen; RegEnumValue(Key, Index, Host, &(HostLen = sizeof(Host)), NULL, &Type, LPBYTE(&History), &(DataLen = 4)) == ERROR_SUCCESS;) {
			if (HostLen && HostLen < 128 && VerifyHost(Host, BYTE(HostLen)) && Type == REG_DWORD && DataLen == 4 && Port && Version >= 700 && Version <= LATEST) {
				return TRUE;
			}
			if (!Delete()) {
				break;
			}
		}
		return FALSE;
	}
	BOOL HostLoop::Delete() {
		return RegDeleteValue(Key, Host) == ERROR_SUCCESS;
	}
	VOID HostLoop::End() CONST {
		RegCloseKey(Key);
	}

	VOID HostLoop::SaveHistory() {
		if (HostLen && Start()) {
			CopyMemoryA(Host, Tibia::Host, HostLen + 1);
			Version = Tibia::Version;
			Port = Tibia::Port;
			Delete();
			RegSetValueEx(Key, Host, 0, REG_DWORD, LPBYTE(&History), 4);
			while (Next(20)) {
				if (!Next(0)) {
					break;
				}
				Delete();
			}
			End();
		}
	}

	BOOL GetAppData(CONST LPTSTR Path) {
		if (!SHGetSpecialFolderPath(NULL, Path, CSIDL_APPDATA, TRUE)) {
			if (!GetEnvironmentVariable(_T("AppData"), Path, MAX_PATH)) {
				return FALSE;
			}
		}
		return TRUE;
	}

	VOID GetInstalledPath(CONST LPTSTR Path) {
		HKEY Key;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Tibia_is1"), 0, KEY_QUERY_VALUE, &Key) == ERROR_SUCCESS) {
			DWORD Size = TLEN(MAX_PATH), Type;
			if (RegQueryValueEx(Key, _T("InstallLocation"), NULL, &Type, LPBYTE(Path), &Size) == ERROR_SUCCESS && Type == REG_SZ) {
				SetCurrentDirectory(Path);
			}
			else if (RegQueryValueEx(Key, _T("Inno Setup: App Path"), NULL, &Type, LPBYTE(Path), &Size) == ERROR_SUCCESS && Type == REG_SZ) {
				SetCurrentDirectory(Path);
			}
			RegCloseKey(Key);
		}
		GetFullPathName(_T("Tibia.exe"), MAX_PATH, Path, NULL);
	}

	WORD GetPathVersion(CONST LPCTSTR Path) { //TODO: detect versions < 730 and > 1099
		DWORD Handle, FileVersionInfoLen = GetFileVersionInfoSize(Path, &Handle);
		if (FileVersionInfoLen) {
			CONST LPBYTE FileVersionInfo = new(std::nothrow) BYTE[FileVersionInfoLen];
			if (FileVersionInfo) {
				if (GetFileVersionInfo(Path, Handle, FileVersionInfoLen, FileVersionInfo)) {
					UINT InfoLen;
					LPVOID Info;
					if (VerQueryValue(FileVersionInfo, _T("\\StringFileInfo\\000004B0\\CompanyName"), &Info, &InfoLen) && !DiffMemory(Info, _T("CipSoft GmbH"), TLEN(13))
						&& VerQueryValue(FileVersionInfo, _T("\\StringFileInfo\\000004B0\\FileDescription"), &Info, &InfoLen) && !DiffMemory(Info, _T("Tibia Player"), TLEN(13))
						&& VerQueryValue(FileVersionInfo, _T("\\StringFileInfo\\000004B0\\ProductName"), &Info, &InfoLen) && !DiffMemory(Info, _T("Tibia Player"), TLEN(13))
						&& VerQueryValue(FileVersionInfo, _T("\\"), &Info, &InfoLen)) {
						CONST WORD Major = HIWORD(((VS_FIXEDFILEINFO*)Info)->dwFileVersionMS);
						CONST WORD Minor = LOWORD(((VS_FIXEDFILEINFO*)Info)->dwFileVersionMS);
						CONST WORD Patch = HIWORD(((VS_FIXEDFILEINFO*)Info)->dwFileVersionLS);
						CONST WORD Build = LOWORD(((VS_FIXEDFILEINFO*)Info)->dwFileVersionLS);
						if (Major >= 7 && Major < 100 && Minor < 10 && Patch < 10 && !Build) {
							WORD Version = Decimal3(Major, Minor, Patch);
							if (Version < 1100) {
								delete[] FileVersionInfo;
								return Version;
							}
						}
					}
				}
				delete[] FileVersionInfo;
			}
		}
		return NULL;
	}

	BOOL ReadSignature(CONST LPTSTR Path, CONST DWORD Index) {
		static CONST TCHAR Datafiles[3][10] = { _T("Tibia.dat"), _T("Tibia.spr"), _T("Tibia.pic") };
		GetFullPathName(Datafiles[Index], MAX_PATH, Path, NULL);
		ReadingFile File;
		return File.Open(Path, OPEN_EXISTING) && File.ReadDword(Signatures[Index]);
	}

	UINT CheckClientPath(CONST LPCTSTR Path) { //TODO: check version if global client
		if (PathIsRelative(Path) || !PathFileExists(Path)) {
			return ERROR_WRONG_TIBIA_EXE;
		}
		TCHAR TestPath[MAX_PATH];
		CopyMemory(TestPath, Path, sizeof(TestPath));
		PathRemoveFileSpec(TestPath);
		if (!SetCurrentDirectory(TestPath)) {
			return ERROR_WRONG_TIBIA_DIR;
		}
		if (!ReadSignature(TestPath, 0)) {
			return ERROR_WRONG_TIBIA_DAT;
		}
		if (!ReadSignature(TestPath, 1)) {
			return ERROR_WRONG_TIBIA_SPR;
		}
		if (!ReadSignature(TestPath, 2)) {
			return ERROR_WRONG_TIBIA_PIC;
		}
		return NULL;
	}

	UINT_PTR CALLBACK PromptPathHook(HWND Dialog, UINT Message, WPARAM Wp, LPARAM Lp) {
		switch (Message) {
		case WM_NOTIFY:
			switch (LPNMHDR(Lp)->code) {
			case CDN_FILEOK:
				LPOPENFILENAME& Info = LPOFNOTIFY(Lp)->lpOFN;
				if (CONST UINT Error = CheckClientPath(Info->lpstrFile)) {
					MainWnd::Progress_Error();
					TCHAR ErrorString[200];
					LoadString(NULL, Error, ErrorString, 200);
					MessageBox(Dialog, ErrorString, Info->lpstrTitle, MB_ICONSTOP);
					MainWnd::Progress_Pause();
					SetWindowLongPtr(Dialog, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
				break;
			}
			break;
		}
		return FALSE;
	}

	BOOL HostLoop::PromptClientPath(CONST LPTSTR Path) {
		if (!PathFileExists(Path)) {
			GetInstalledPath(Path);
		}
		TCHAR Title[170];
		CopyMemory(Title + LoadString(NULL, TITLE_SELECT_TIBIA, Title, 30), Host, TLEN(140));
		TCHAR Format[40];
		CopyMemory(Format + LoadString(NULL, FILETYPE_EXE, Format, 24), _T(" (*.exe)\0*.exe\0"), TLEN(16));
		OPENFILENAME OpenFileName;
		OpenFileName.lStructSize = sizeof(OPENFILENAME);
		OpenFileName.hwndOwner = MainWnd::Handle;
		OpenFileName.lpstrFilter = Format;
		OpenFileName.lpstrCustomFilter = NULL;
		OpenFileName.nFilterIndex = 1;
		OpenFileName.lpstrFile = Path;
		OpenFileName.nMaxFile = MAX_PATH;
		OpenFileName.lpstrFileTitle = NULL;
		OpenFileName.lpstrInitialDir = NULL;
		OpenFileName.lpstrTitle = Title;
		OpenFileName.lpstrDefExt = _T("exe");
		OpenFileName.lpfnHook = PromptPathHook;
		OpenFileName.lCustData = LPARAM(this);
		OpenFileName.Flags = OFN_DONTADDTORECENT | OFN_ENABLEHOOK | OFN_ENABLESIZING | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		OpenFileName.FlagsEx = 0;
		if (!GetOpenFileName(&OpenFileName)) {
			return FALSE;
		}
		return TRUE;
	}

	VOID HostLoop::ImportLegacy() { // TODO: prune unused entries from \client (mayber another thread)
		if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Tibia\\Folder"), 0, KEY_QUERY_VALUE, &Key) == ERROR_SUCCESS) {
			for (DWORD Index = 0, PathLen; RegEnumValue(Key, Index, Host, &(PathLen = sizeof(Host)), NULL, NULL, NULL, NULL) == ERROR_SUCCESS; Index++) {
				if (PathLen < MAX_PATH && !PathIsRelative(Host) && GetPathVersion(Host)) {
					HKEY NewKey;
					if (RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\Tibia\\Client"), 0, NULL, NULL, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &NewKey, NULL) == ERROR_SUCCESS) {
						SetVersionString(Version);
						RegSetValueEx(NewKey, VersionString, NULL, REG_SZ, LPBYTE(Host), TLEN(PathLen));
						RegCloseKey(NewKey);
					}
				}
			}
			RegCloseKey(Key);
			RegDeleteKey(HKEY_CURRENT_USER, _T("Software\\Tibia\\Folder"));
		}
	}

	BOOL HostLoop::GetClientPath(CONST LPTSTR Path) {
		ImportLegacy();
		CopyMemory(Host, VersionString, sizeof(VersionString));
		if (HostLen) {
			SIZE_T Pos = _tcslen(VersionString);
			CopyMemory(Host + Pos, _T(" ["), TLEN(2));
			CopyMemoryA(Host + Pos + 2, Tibia::Host, HostLen + 1);
			CopyMemory(Host + Pos + HostLen + 2, _T("]"), TLEN(2));
		}
		if (RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\Tibia\\Client"), 0, NULL, NULL, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &Key, NULL) == ERROR_SUCCESS) {
			DWORD Type, PathLen;
			if (RegQueryValueEx(Key, Host, NULL, &Type, LPBYTE(Path), &(PathLen = TLEN(MAX_PATH))) == ERROR_SUCCESS) {
				if (GetKeyState(VK_SHIFT) >= 0) {
					if (!CheckClientPath(Path)) {
						End();
						SaveHistory();
						return TRUE;
					}
					Delete();
				}
			}
			else if (HostLen) {
				RegQueryValueEx(Key, VersionString, NULL, &Type, LPBYTE(Path), &(PathLen = TLEN(MAX_PATH)));
			}
			if (PromptClientPath(Path)) {
				RegSetValueEx(Key, Host, NULL, REG_SZ, LPBYTE(Path), TLEN(_tcslen(Path)));
				End();
				SaveHistory();
				return TRUE;
			}
			End();
			return FALSE;
		}
		if (PromptClientPath(Path)) {
			SaveHistory();
			return TRUE;
		}
		return FALSE;
	}

	BOOL ChangeConfigHost(CONST LPTSTR Path) {
		GetFullPathName(_T("Tibia.cfg"), MAX_PATH, Path, NULL);
		WritingFile File;
		if (File.Open(Path, OPEN_ALWAYS)) {
			File.Append();
			CONST CHAR ServerString[45] = {
				'\r', '\n', 'S', 'e', 'r', 'v', 'e', 'r', 'A', 'd', 'd', 'r', 'e', 's', 's',
				'=', '\"', '1', '2', '7', '.', '0', '.', '0', '.', '1', '\"',
				'\r', '\n', 'S', 'e', 'r', 'v', 'e', 'r', 'P', 'o', 'r', 't',
				'=', CharDigits(5, Proxy::Login.Port)
			};
			if (File.Write(ServerString, 45)) {
				return TRUE;
			}
		}
		return FALSE;
	}

	BOOL RemoteThread(CONST LPVOID Call, CONST LPVOID Param, CONST LPVOID Result) {
		if (HANDLE Remote = CreateRemoteThread(Proc.hProcess, NULL, NULL, LPTHREAD_START_ROUTINE(Call), Param, NULL, NULL)) {
			if (!WaitForSingleObject(Remote, 2000)) {
				if (GetExitCodeThread(Remote, LPDWORD(Result))) {
					return CloseHandle(Remote);
				}
			}
			CloseHandle(Remote);
		}
		return FALSE;
	}

	BOOL GetTibiaSections(LPBYTE& BaseAddress, IMAGE_SECTION_HEADER* Sections) {
		if (Version < 910 || LOBYTE(LOWORD(GetVersion())) < 6) { //WinXP doesn't have ASLR, and RemoteThread causes problems
			BaseAddress = LPBYTE(0x400000);
		}
		else if (!RemoteThread(GetModuleHandleA, NULL, &BaseAddress) || !BaseAddress) {
			return FALSE;
		}
		IMAGE_DOS_HEADER DosHeader;
		if (ReadProcessMemory(Proc.hProcess, BaseAddress, &DosHeader, sizeof(IMAGE_DOS_HEADER), NULL) && DosHeader.e_magic == IMAGE_DOS_SIGNATURE) {
			LPBYTE HeaderAddress = BaseAddress + DosHeader.e_lfanew + 4;
			IMAGE_FILE_HEADER ImageHeader;
			if (ReadProcessMemory(Proc.hProcess, HeaderAddress, &ImageHeader, sizeof(IMAGE_FILE_HEADER), NULL) && ImageHeader.NumberOfSections >= 3) {
				HeaderAddress += sizeof(IMAGE_FILE_HEADER) + ImageHeader.SizeOfOptionalHeader;
				if (ReadProcessMemory(Proc.hProcess, HeaderAddress, Sections, sizeof(IMAGE_SECTION_HEADER) * 3, NULL) && !DiffMemory(Sections[SECTION_TEXT].Name, ".text", 6) && !DiffMemory(Sections[SECTION_RDATA].Name, ".rdata", 7) && !DiffMemory(Sections[SECTION_DATA].Name, ".data", 6)) {
					return TRUE;
				}
			}
		}
		return FALSE;
	}

	BOOL MemorySearch(LPBYTE &Address, CONST IMAGE_SECTION_HEADER &Section, CONST LPCSTR Find, CONST SIZE_T Len, DWORD Occurrence) {
		CONST LPSTR Buffer = new(std::nothrow) CHAR[Section.Misc.VirtualSize];
		if (Buffer) {
			if (ReadProcessMemory(Proc.hProcess, Address + Section.VirtualAddress, Buffer, Section.Misc.VirtualSize, NULL)) {
				for (SIZE_T Pos = 0; Pos + Len < Section.Misc.VirtualSize; Pos++) {
					for (SIZE_T Found = 0; Find[Found] == Buffer[Pos + Found]; Found++) {
						if (Found == Len) {
							if (Occurrence--) {
								Pos += Len;
								break;
							}
							Address += Section.VirtualAddress + Pos;
							delete[] Buffer;
							return TRUE;
						}
					}
				}
			}
			delete[] Buffer;
		}
		return FALSE;
	}
	BOOL MemorySearchAddress(LPBYTE &Address, CONST IMAGE_SECTION_HEADER &Section, CONST BYTE *CONST Find, CONST SIZE_T Len) {
		CONST LPBYTE Buffer = new(std::nothrow) BYTE[Section.Misc.VirtualSize];
		if (Buffer) {
			if (ReadProcessMemory(Proc.hProcess, Address + Section.VirtualAddress, Buffer, Section.Misc.VirtualSize, NULL)) {
				for (SIZE_T Pos = 4; Pos + Len < Section.Misc.VirtualSize; Pos++) {
					for (SIZE_T Found = 0; Find[Found] == Buffer[Pos + Found]; Found++) {
						if (Found == Len) {
							Address = *(LPBYTE*)(Buffer + Pos - 4);
							delete[] Buffer;
							return TRUE;
						}
					}
				}
			}
			delete[] Buffer;
		}
		return FALSE;
	}
	BOOL WriteProtectedMemory(CONST LPVOID Address, CONST LPCVOID Data, CONST SIZE_T Size) {
		DWORD OldProtect;
		if (VirtualProtectEx(Proc.hProcess, Address, Size, PAGE_READWRITE, &OldProtect)) {
			if (WriteProcessMemory(Proc.hProcess, Address, Data, Size, NULL)) {
				return VirtualProtectEx(Proc.hProcess, Address, Size, OldProtect, &OldProtect);
			}
			VirtualProtectEx(Proc.hProcess, Address, Size, OldProtect, &OldProtect);
		}
		return FALSE;
	}

	BOOL ChangeConfig(CONST LPTSTR Path, CONST LPTSTR FileName, LPBYTE Address, CONST IMAGE_SECTION_HEADER *Sections) {
		FileName[0] = NULL;
		{
			CONST BOOL AppData = PathFileExists(Path);
			ListAssign(4, Split4(FileName, 0, 6, 7, 8), List4('T', 'c', 'f', 'g'));
			ListAssign(4, Split4(FileName, 1, 2, 3, 4), CharDigits(4, Version));
			if (AppData && !PathFileExists(Path)) {
				TCHAR TryPath[MAX_PATH];
				CONST LPTSTR TryFileName = TryPath + (FileName - Path);
				CopyMemory(TryPath, Path, TLEN(FileName - Path + 10));
				WORD TryVersion = Version - 1;
				do {
					if (TryVersion < 800) {
						CopyMemory(TryFileName + 1, _T("ibia"), TLEN(4));
						CopyFile(TryPath, Path, TRUE);
						break;
					}
					ListAssign(4, Split4(TryFileName, 1, 2, 3, 4), CharDigits(4, TryVersion));
					TryVersion--;
				} while (!CopyFile(TryPath, Path, TRUE) && GetLastError() == ERROR_FILE_NOT_FOUND);
			}
		}
		if (!MemorySearch(Address, Sections[SECTION_RDATA], "Tibia.cfg", 9, 0)) {
			return FALSE;
		}
		CHAR Buffer[4] = { CharDigits(4, Version) };
		return WriteProtectedMemory(Address + 1, Buffer, 4);
	}

	BOOL CALLBACK GetWindow(HWND TestWnd, LPARAM) {
		TCHAR ClassName[13];
		if (GetClassName(TestWnd, ClassName, 13) == 11 && !DiffMemory(ClassName, _T("TibiaClient"), TLEN(11))) {
			Wnd = TestWnd;
			return FALSE;
		}
		return TRUE;
	}
	BOOL CALLBACK GetErrorWindow(HWND TestWnd, LPARAM) {
		TCHAR Buffer[15];
		if (GetClassName(TestWnd, Buffer, 15) == 6 && !DiffMemory(Buffer, _T("#32770"), TLEN(6))) {
			if (GetWindowText(TestWnd, Buffer, 15) == 13 && !DiffMemory(Buffer, _T("Tibia - Error"), TLEN(13))) {
				return FALSE;
			}
		}
		return TRUE;
	}

	LPCTSTR GetIcon() {
		return MAKEINTRESOURCE(Version >= 800 ? (HostLen ? 6 : 5) : (HostLen ? 4 : 3));
	}

	BOOL ActivateLocalProxy(CONST LPBYTE PortAddress) {
		Proxy::State = Proxy::WAIT_LOCALPROXY;
		MainWnd::Done();
		EnableWindow(MainWnd::Handle, FALSE);
		MainWnd::Progress_Pause();
		TCHAR MessageString[200];
		LoadString(NULL, MESSAGE_PROXY, MessageString, 200);
		MessageBox(Wnd, MessageString, MainWnd::Title, MB_ICONINFORMATION);
		MainWnd::Progress_Start();
		EnableWindow(MainWnd::Handle, TRUE);
		MainWnd::Wait();
		Proxy::State = Proxy::WAITING;
		if (!ReadProcessMemory(Proc.hProcess, PortAddress, &Proxy::Port, 2, NULL) || !Proxy::Port) {
			Proxy::Port = TRUE;
			return FALSE;
		}
		return TRUE;
	}

	BOOL ChangeRSA(CONST LPBYTE RSAAddress) {
		return CustomClient || WriteProtectedMemory(RSAAddress, RSA::Modulus_OTS, 310);
	}

	BOOL ChangeMemoryHost(LPBYTE Address, CONST IMAGE_SECTION_HEADER *Sections) {
		CustomClient = FALSE;
		LPBYTE RSAAddress;
		if (Version >= 1011) {
			if (!MemorySearch(RSAAddress = Address, Sections[SECTION_RDATA], RSA::Modulus_861, 309, 0)) {
				if (!MemorySearch(RSAAddress = Address, Sections[SECTION_RDATA], RSA::Modulus_OTS, 309, 0)) {
					return FALSE;
				}
				CustomClient = TRUE;
			}
			DWORD PortOffset;
			if (Version >= 1050) { //search signature from Jo3Bingham IPchanger
				CONST BYTE Find[] = { 0xB8, 0xAB, 0xAA, 0xAA, 0x2A, 0xF7, 0xEE, 0xBE, 0x55, 0x55, 0x55, 0x05 };
				if (!MemorySearchAddress(Address, Sections[SECTION_TEXT], Find, 11)) {
					return FALSE;
				}
				PortOffset = 20;
			}
			else {
				if (Version >= 1012) { //search signature modded from OTLand IPchanger
					CONST BYTE Find[] = { 0xB8, 0x93, 0x24, 0x49, 0x92, 0xF7, 0xE9, 0x03, 0xD1, 0xC1, 0xFA, 0x05, 0x8B, 0xC2, 0xC1, 0xE8, 0x1F, 0x03, 0xC2, 0x74 };
					if (!MemorySearchAddress(Address, Sections[SECTION_TEXT], Find, 19)) {
						return FALSE;
					}
				}
				else { //hardcoded addresses for 10.11
					CONST BYTE ClientIPCheckFunctionNOP[] = { 0x83, 0x38, 0x00, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x95, 0xD0, 0xC3 };
					if (!WriteProcessMemory(Proc.hProcess, Address + 0x0E8663, ClientIPCheckFunctionNOP, 12, NULL)) {
						return FALSE;
					}
					Address += 0x40F31C;
				}
				PortOffset = 28;
			}
			{
				LPBYTE EndAddress;
				if (!ReadProcessMemory(Proc.hProcess, Address + 4, &EndAddress, 4, NULL)) {
					return FALSE;
				}
				if (!ReadProcessMemory(Proc.hProcess, Address, &Address, 4, NULL)) {
					return FALSE;
				}
				if (Proxy::Port && !ActivateLocalProxy(Address + 20 + PortOffset)) {
					return FALSE;
				}
				if (!ChangeRSA(RSAAddress)) {
					return FALSE;
				}
				while (Address < EndAddress) {
					DWORD ServerLen;
					if (!ReadProcessMemory(Proc.hProcess, Address + 24, &ServerLen, 4, NULL)) {
						return FALSE;
					}
					LPBYTE HostAddress;
					if (ServerLen > 16) {
						if (!ReadProcessMemory(Proc.hProcess, Address += 4, &HostAddress, 4, NULL)) {
							return FALSE;
						}
					}
					else {
						HostAddress = Address += 4;
					}
					if (!WriteProcessMemory(Proc.hProcess, HostAddress, "127.0.0.1", 10, NULL)) {
						return FALSE;
					}
					if (!WriteProcessMemory(Proc.hProcess, Address += 16, &(ServerLen = 9), 4, NULL)) {
						return FALSE;
					}
					if (!WriteProcessMemory(Proc.hProcess, Address += PortOffset, &(Proxy::Login.Port), 2, NULL)) {
						return FALSE;
					}
					Address += 8;
				}
			}
		}
		else if (Version >= 711) {
			DWORD HostCount;
			if (Version >= 800) {
				if (!MemorySearch(RSAAddress = Address, Sections[SECTION_RDATA], Version >= 861 ? RSA::Modulus_861 : RSA::Modulus_771, 309, 0)) {
					if (!MemorySearch(RSAAddress = Address, Sections[SECTION_RDATA], RSA::Modulus_OTS, 309, 0)) {
						return FALSE;
					}
					CustomClient = TRUE;
				}
				if (!MemorySearch(Address, Sections[SECTION_DATA], "login01.tibia.com", 17, 0)) {
					if (!MemorySearch(Address, Sections[SECTION_DATA], Host, HostLen, 0)) {
						return FALSE;
					}
					CustomClient = TRUE;
				}
				HostCount = 10;
			}
			else {
				if (Version >= 761) {
					if (!MemorySearch(RSAAddress = Address, Sections[SECTION_RDATA], Version >= 771 ? RSA::Modulus_771 : RSA::Modulus_761, 309, 0)) {
						if (!MemorySearch(RSAAddress = Address, Sections[SECTION_RDATA], RSA::Modulus_OTS, 309, 0)) {
							return FALSE;
						}
					}
				}
				else {
					RSAAddress = NULL;
				}
				if (!MemorySearch(Address, Sections[SECTION_DATA], "test.cipsoft.com", 16, Version >= 730 ? 0 : 1)) {
					if (!MemorySearch(Address, Sections[SECTION_DATA], Host, HostLen, Version >= 730 ? 0 : 1)) {
						return FALSE;
					}
					CustomClient = TRUE;
				}
				HostCount = 5;
			}
			if (Proxy::Port && !ActivateLocalProxy(Address + 212)) {
				return FALSE;
			}
			if (RSAAddress && !ChangeRSA(RSAAddress)) {
				return FALSE;
			}
			while (HostCount--) {
				if (!WriteProcessMemory(Proc.hProcess, Address, "127.0.0.1", 10, NULL)) {
					return FALSE;
				}
				if (!WriteProcessMemory(Proc.hProcess, Address += 100, &(Proxy::Login.Port), 2, NULL)) {
					return FALSE;
				}
				Address += 12;
			}
		}
		else if (Proxy::Port) {
			CONST CHAR Find[] = { Split2((CHAR*)&Proxy::Login.Port, 0, 1), 0, 0, '1', '2', '7', '.', '0', '.', '0', '.', '1', 0 };
			if (!MemorySearch(Address, Sections[SECTION_DATA], Find, 13, 0)) {
				return FALSE;
			}
			if (!ActivateLocalProxy(Address)) {
				return FALSE;
			}
			if (Proxy::Port == Proxy::Login.Port) {
				Proxy::Port = PORT;
			}
			if (!WriteProcessMemory(Proc.hProcess, Address, Find, 14, NULL)) {
				return FALSE;
			}
		}
		return TRUE;
	}

	VOID AutoCloseDialogs() {
		PostMessage(Wnd, WM_CHAR, VK_ESCAPE, 1);
		PostMessage(Wnd, WM_CHAR, VK_ESCAPE, 1);
		PostMessage(Wnd, WM_CHAR, VK_ESCAPE, 1); //7.11 to 7.3 needs this
	}
	VOID ClickEnterGame() {
		if (Version >= 1120) {
			PostMessage(Wnd, WM_CHAR, VK_ESCAPE, 1);
		}
		RECT ClientArea;
		if (GetClientRect(Wnd, &ClientArea)) {
			POINT Click;
			if (Version >= 1038) {
				Click = { 88, -200 };
			}
			else if (Version >= 920) {
				Click = { 88, -168 };
			}
			else {
				Click = { 128, -224 };
			}
			LPVOID IsProcessDPIAware = GetProcAddress(GetModuleHandle(_T("user32.dll")), "IsProcessDPIAware");
			if (IsProcessDPIAware) { // Function abscense = XP, that means no scaling
				if (RemoteThread(IsProcessDPIAware, NULL, &IsProcessDPIAware)) { // Function failure = assume no scaling
					if (!IsProcessDPIAware) { // If Tibia is not DPI-aware, we have to scale mouse position to send a click
						Click = { (Click.x / 8) * MainWnd::Base.x, (Click.y / 8) * MainWnd::Base.y };
					}
				}
			}
			Click.y += ClientArea.bottom;
			LPARAM ClickParam = MAKELPARAM(Click.x, Click.y);
			PostMessage(Wnd, WM_LBUTTONDOWN, NULL, ClickParam);
			PostMessage(Wnd, WM_LBUTTONUP, NULL, ClickParam);
		}
	}
	VOID EnterAutoPlay() {
		PostMessage(Wnd, WM_CHAR, CharDigit(3, timeGetTime()), NULL);
		PostMessage(Wnd, WM_CHAR, CharDigit(1, timeGetTime()), NULL);
		PostMessage(Wnd, WM_CHAR, CharDigit(2, timeGetTime()), NULL);
		PostMessage(Wnd, WM_CHAR, CharDigit(4, timeGetTime()), NULL);
		PostMessage(Wnd, WM_CHAR, CharDigit(5, timeGetTime()), NULL);
		PostMessage(Wnd, WM_CHAR, CharDigit(3, timeGetTime()), NULL);
		PostMessage(Wnd, WM_CHAR, CharDigit(1, timeGetTime()), NULL);
		PostMessage(Wnd, WM_CHAR, CharDigit(2, timeGetTime()), NULL);
		PostMessage(Wnd, WM_CHAR, VK_RETURN, NULL);
	}
	VOID AutoPlay() {
		if (DWORD AutoPlay = MainWnd::GetAutoPlay()) {
			if (Running && Proxy::State == Proxy::WAITING) {
				EnableWindow(Wnd, FALSE);
				AutoCloseDialogs();
				ClickEnterGame();
				Sleep(AutoPlay * 3);
				EnterAutoPlay();
				EnableWindow(Wnd, TRUE);
			}
			Video::Start();
		}
	}
	VOID AutoPlayCharlist() {
		DWORD AutoPlay = MainWnd::GetAutoPlay();
		if (!CustomClient && Version >= 1098) {
			Sleep(AutoPlay ? AutoPlay : 100);
			PostMessage(Wnd, WM_CHAR, VK_ESCAPE, NULL);
		}
		if (AutoPlay) {
			Sleep(AutoPlay);
			PostMessage(Wnd, WM_CHAR, VK_RETURN, NULL);
		}
	}

	VOID CreateVersionMenu() {
		if (CONST HMENU Menu = CreatePopupMenu()) {
			TCHAR MenuString[128];
			CopyMemory(MenuString + LoadString(NULL, MENU_VERSION, MenuString, 20), VersionString, sizeof(VersionString));
			AppendMenu(Menu, MF_STRING | MF_GRAYED, IDVERSION, MenuString);
			if (HostLen) {
				CopyMemoryA(MenuString, Host, HostLen + 1);
			}
			else {
				LoadString(NULL, MENU_HOST, MenuString, 128);
			}
			AppendMenu(MainWnd::Menu, MF_POPUP, UINT_PTR(Menu), MenuString);
			DrawMenuBar(MainWnd::Handle);
		}
	}

	UINT WINAPI RunningProc(LPVOID Started) {
		SetEvent(Started);
		if (!WaitForSingleObjectEx(Proc.hProcess, INFINITE, TRUE)) {
			PostMessage(MainWnd::Handle, WM_TIBIACLOSED, WPARAM(Running), LPARAM(Proc.hProcess));
		}
		CloseHandle(Proc.hProcess);
		return 0;
	}

	VOID CALLBACK CloseProc(ULONG_PTR) {
		AutoCloseDialogs();
		SendMessage(Wnd, WM_CLOSE, NULL, NULL);
		PostThreadMessage(Proc.dwThreadId, WM_QUIT, EXIT_FAILURE, NULL);
		if (WaitForSingleObject(Proc.hProcess, 1000)) {
			TerminateProcess(Proc.hProcess, EXIT_FAILURE);
		}
	}

	VOID Close() {
		Video::HandleTibiaClosed();
		Proxy::HandleTibiaClosed();
		QueueUserAPC(CloseProc, Running, NULL);
		EnableMenuItem(MainWnd::Menu, IDPROXY, MF_ENABLED);
		if (Video::Last) {
			EnableMenuItem(MainWnd::Menu, IDVERSION, MF_GRAYED);
		}
		else {
			DeleteMenu(MainWnd::Menu, TOP_VERSION, MF_BYPOSITION);
			DrawMenuBar(MainWnd::Handle);
		}
		WaitForSingleObject(Running, INFINITE);
		CloseHandle(Running);
		Running = NULL;
	}
	VOID Start() {
		{
			LPBYTE BaseAddress;
			IMAGE_SECTION_HEADER Sections[3];
			{
				TCHAR Path[MAX_PATH];
				GetFullPathName(_T("Tibia.exe"), MAX_PATH, Path, NULL);
				if (!HostLoop().GetClientPath(Path)) {
					return;
				}
				MainWnd::Wait();
				MainWnd::Progress_Start();
				if (!Proxy::Start()) {
					return ErrorBox(ERROR_START_SERVERS, TITLE_START_TIBIA);
				}
				{
					TCHAR CmdLine[MAX_PATH] = _T("Tibia.exe gamemaster");
					STARTUPINFO Start;
					Start.cb = sizeof(STARTUPINFO);
					Start.lpReserved = NULL;
					Start.lpDesktop = NULL;
					Start.lpTitle = NULL;
					Start.dwFlags = STARTF_FORCEONFEEDBACK | STARTF_USESHOWWINDOW;
					Start.wShowWindow = SW_SHOWNORMAL;
					Start.cbReserved2 = NULL;
					Start.lpReserved2 = NULL;
					if (!CreateProcess(Path, CmdLine, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &Start, &Proc)) {
						Proxy::Stop();
						return ErrorBox(ERROR_START_TIBIA, TITLE_START_TIBIA);
					}
				}
				if (!GetTibiaSections(BaseAddress, Sections)) {
					TerminateProcess(Proc.hProcess, EXIT_FAILURE);
					CloseHandle(Proc.hThread);
					CloseHandle(Proc.hProcess);
					Proxy::Stop();
					return ErrorBox(ERROR_GET_BASE_ADDRESS, TITLE_START_TIBIA);
				}
				if (Version >= 800) {
					LPTSTR FileName;
					if (GetAppData(Path)) {
						PathAppend(Path, _T("Tibia\\error.txt"));
						FileName = PathFindFileName(Path);
					}
					else {
						GetFullPathName(_T("error.txt"), MAX_PATH, Path, &FileName);
					}
					DeleteFile(Path);
					if (!ChangeConfig(Path, FileName, BaseAddress, Sections)) {
						TerminateProcess(Proc.hProcess, EXIT_FAILURE);
						CloseHandle(Proc.hThread);
						CloseHandle(Proc.hProcess);
						Proxy::Stop();
						return ErrorBox(ERROR_CHANGE_TIBIA_CONFIG, TITLE_START_TIBIA);
					}
				}
				else if (Version >= 761) {
					GetFullPathName(_T("error.txt"), MAX_PATH, Path, NULL);
					DeleteFile(Path);
				}
				else if (Version < 711) {
					if (!ChangeConfigHost(Path)) {
						TerminateProcess(Proc.hProcess, EXIT_FAILURE);
						CloseHandle(Proc.hThread);
						CloseHandle(Proc.hProcess);
						Proxy::Stop();
						return ErrorBox(ERROR_CHANGE_TIBIA_HOST, TITLE_START_TIBIA);
					}
				}
				ResumeThread(Proc.hThread);
				CloseHandle(Proc.hThread);
			}
			WaitForInputIdle(Proc.hProcess, INFINITE);
			Wnd = NULL;
			if (EnumThreadWindows(Proc.dwThreadId, GetWindow, NULL) || !Wnd) {
				TerminateProcess(Proc.hProcess, EXIT_FAILURE);
				CloseHandle(Proc.hProcess);
				Proxy::Stop();
				return ErrorBox(ERROR_FIND_TIBIA_WINDOW, TITLE_START_TIBIA);
			}
			for (DWORD Time = timeGetTime(); !IsWindowVisible(Wnd); Sleep(1)) {
				if (!EnumThreadWindows(Proc.dwThreadId, GetErrorWindow, NULL)) {
					CloseHandle(Proc.hProcess);
					Proxy::Stop();
					MainWnd::Done();
					MainWnd::Progress_Stop();
					return;
				}
				if (!IsWindow(Wnd) || timeGetTime() - Time > TIMEOUT) {
					TerminateProcess(Proc.hProcess, EXIT_FAILURE);
					CloseHandle(Proc.hProcess);
					Proxy::Stop();
					return ErrorBox(ERROR_WAIT_TIBIA_WINDOW, TITLE_START_TIBIA);
				}
			}
			EnableWindow(Wnd, FALSE);
			PostMessage(Wnd, WM_CHAR, VK_ESCAPE, 1);
			{
				CONST HMODULE Module = GetModuleHandle(NULL);
				CONST LPCTSTR Icon = GetIcon();
				SetWindowIcon(Wnd, ICON_SMALL, LoadSmallIcon(Module, Icon));
				SetWindowIcon(Wnd, ICON_BIG, LoadIcon(Module, Icon));
			}
			if (!ChangeMemoryHost(BaseAddress, Sections)) {
				TerminateProcess(Proc.hProcess, EXIT_FAILURE);
				CloseHandle(Proc.hProcess);
				Proxy::Stop();
				return ErrorBox(ERROR_CHANGE_TIBIA_HOST, TITLE_START_TIBIA);
			}
		}
		{
			HANDLE Started = CreateEvent(NULL, TRUE, FALSE, NULL);
			Running = HANDLE(_beginthreadex(NULL, 1, RunningProc, Started, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL));
			if (!Running) {
				TerminateProcess(Proc.hProcess, EXIT_FAILURE);
				CloseHandle(Proc.hProcess);
				CloseHandle(Started);
				Proxy::Stop();
				return ErrorBox(ERROR_THREAD_WAITER, TITLE_START_TIBIA);
			}
			ClickEnterGame();
			EnableMenuItem(MainWnd::Menu, IDPROXY, MF_GRAYED);
			if (!Video::Last) {
				CreateVersionMenu();
			}
			else if (DWORD AutoPlay = MainWnd::GetAutoPlay()) {
				Sleep(AutoPlay * 3);
				EnterAutoPlay();
			}
			EnableMenuItem(MainWnd::Menu, IDVERSION, MF_ENABLED);
			WaitForSingleObject(Started, INFINITE);
			CloseHandle(Started);
		}
		EnableWindow(Wnd, TRUE);
		MainWnd::Done();
		MainWnd::Progress_Stop();
	}

	VOID OpenVersionMenu() {
		if (!Running) {
			CreateVersionMenu();
		}
	}
	VOID CloseVersionMenu() {
		if (!Running) {
			DeleteMenu(MainWnd::Menu, TOP_VERSION, MF_BYPOSITION);
			DrawMenuBar(MainWnd::Handle);
		}
	}

	VOID Flash() {
		if (IsIconic(Wnd) || !SetForegroundWindow(Wnd)) {
			FlashWindow(Wnd, TRUE);
		}
	}

	BYTE ParseURLHost(CONST LPCTSTR Host) {
		BYTE Len;
		for (Len = 0; Host[Len]; Len++) {
			if (Len == 127) {
				return 0;
			}
			if (Host[Len] == '-') {
				if (!Host[Len+1]) {
					return 0;
				}
			}
			else if (Host[Len] != '.' && !IsCharDigit(Host[Len]) && Host[Len] != '_' && NotLowLetter(Host[Len])) {
				return 0;
			}
		}
		return Len;
	}
	WORD ParseURLPort(CONST LPCTSTR PortStr) {
		if (PortStr[0]) {
			if (!PortStr[1]) {
				if (IsCharDigit(PortStr[0])) {
					return CharToDigit(PortStr[0]);
				}
			}
			else if (!PortStr[2]) {
				if (AreCharDigits(2, Split2(PortStr, 0, 1))) {
					return DecimalChars(2, Split2(PortStr, 0, 1));
				}
			}
			else if (!PortStr[3]) {
				if (AreCharDigits(3, Split3(PortStr, 0, 1, 2))) {
					return DecimalChars(3, Split3(PortStr, 0, 1, 2));
				}
			}
			else if (!PortStr[4]) {
				if (AreCharDigits(4, Split4(PortStr, 0, 1, 2, 3))) {
					return DecimalChars(4, Split4(PortStr, 0, 1, 2, 3));
				}
			}
			else if (!PortStr[5]) {
				if (AreCharDigits(5, Split5(PortStr, 0, 1, 2, 3, 4))) {
					return DecimalChars(5, Split5(PortStr, 0, 1, 2, 3, 4));
				}
			}
		}
		return 0;
	}
	WORD ParseURLVersion(CONST LPCTSTR VersionStr) {
		if (ListOperation(2, Split2(VersionStr, 0, 1), &&,)) {
			if (!VersionStr[2]) {
				if (AreCharDigits(2, Split2(VersionStr, 0, 1))) {
					return DecimalChars(2, Split2(VersionStr, 0, 1)) * 10;
				}
			}
			else if (!VersionStr[3]) {
				if (AreCharDigits(3, Split3(VersionStr, 0, 1, 2))) {
					if (VersionStr[0] < '7') {
						return DecimalChars(3, Split3(VersionStr, 0, 1, 2)) * 10;
					}
					return DecimalChars(3, Split3(VersionStr, 0, 1, 2));
				}
			}
			else if (!VersionStr[4]) {
				if (AreCharDigits(4, Split4(VersionStr, 0, 1, 2, 3))) {
					return DecimalChars(4, Split4(VersionStr, 0, 1, 2, 3));
				}
			}
		}
		return 0;
	}

	VOID StartURL(CONST LPTSTR Url) {
		LPTSTR Part[6]; //otserv:p0/p1/p2 = host/p3 = port/p4 = version[/p5 = rest]
		Part[0] = Url + 7;
		if (!DiffMemory(Part[0], _T("loader"), TLEN(7))) {
			if (!Loader().Run(MainWnd::Handle, TITLE_LOADER)) {
				return;
			}
		}
		else if (DiffMemory(Part[0], _T("global"), TLEN(7))) {
			for (UINT i = 1; i < 6; i++) {
				if(Part[i] = _tcschr(Part[i-1], '/')) {
					(Part[i]++)[0] = NULL;
				}
				else {
					Part[5] = _tcschr(Part[i-1], NULL);
					for (; i < 5; i++) {
						Part[i] = Part[5];
					}
					break;
				}
			}
			if (Part[0][0] || Part[1][0]) {
				ErrorBox(ERROR_URL, TITLE_START_TIBIA);
				return;
			}
			BYTE HostLen = ParseURLHost(Part[2]);
			if (!HostLen) {
				ErrorBox(ERROR_URL, TITLE_START_TIBIA);
				return;
			}
			WORD Port = ParseURLPort(Part[3]);
			if (!Port) {
				ErrorBox(ERROR_URL, TITLE_START_TIBIA);
				return;
			}
			WORD Version = ParseURLVersion(Part[4]);
			if (Version < 700) {
				ErrorBox(ERROR_URL, TITLE_START_TIBIA);
				return;
			}
			if (Version > LATEST) {
				Version = LATEST;
			}
			SetVersionString(Version);
			SetHost(Version, HostLen, Part[2], Port);
		}
		Start();
		if (Wnd) {
			Flash();
			if (HostLen || Proxy::Port) {
				MainWnd::MinimizeToTray();
			}
		}
	}

	VOID CloseDialogs() {
		INT Timeout;
		if (SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, NULL, &Timeout, NULL)) {
			SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, Timeout, NULL, NULL);
		}
		if (GetForegroundWindow() != Wnd) {
			SendNotifyMessage(Wnd, WM_CHAR, VK_ESCAPE, 1);
		}
	}
	VOID Lock() {
		SendNotifyMessage(Wnd, WM_CHAR, VK_ESCAPE, 1);
		LockWindowUpdate(Wnd);
	}
	VOID Unlock() {
		LockWindowUpdate(NULL);
	}
	VOID Redraw() {
		Unlock();
		SendMessageTimeout(Wnd, WM_CHAR, VK_ESCAPE, 1, SMTO_BLOCK, 1000, NULL);
		RedrawWindow(Wnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}