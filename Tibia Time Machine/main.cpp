#include "main.h"
#include "video.h"
#include "tibia.h"
#include "proxy.h"
#include "parser.h"
#include "loader.h"
#include "bigword.h"

namespace MainWnd {
	UINT TaskbarRestart;
	ITaskbarList3* TaskbarButton = NULL;
	DWORD Progress = 0;
	DWORD Progress_Segment = 0;
	DWORD Progress_Segments = 1;

	POINT Base;

	HWND Handle;

	HWND ScrollPlayed;
	HWND ButtonSlowDown;
	HWND ButtonSpeedUp;
	HWND ListSessions;
	HWND LabelTime;
	HWND StatusTime;
	HWND ButtonSub;
	HWND ButtonMain;

	HMENU Menu;
	HBRUSH ListBG;

	INT Wheel = 0;

	SCROLLINFO ScrollInfo = { sizeof(SCROLLINFO), SIF_ALL | SIF_DISABLENOSCROLL, 0, 59, 60, 0, 0 };

	VOID SetListBG() {
		ListBG = GetSysColorBrush(COLOR_WINDOW);
		if (HBITMAP Bitmap = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(1))) {
			if (HDC ListDC = GetDC(ListSessions)) {
				if (HDC OrigDC = CreateCompatibleDC(ListDC)) {
					SelectObject(OrigDC, Bitmap);
					RECT ListRect;
					if (GetClientRect(ListSessions, &ListRect)) {
						if (HBITMAP NewBitmap = CreateCompatibleBitmap(ListDC, ListRect.right, ListRect.bottom)) {
							if (HDC DestDC = CreateCompatibleDC(ListDC)) {
								SelectObject(DestDC, NewBitmap);
								SelectObject(DestDC, ListBG);
								ExtFloodFill(DestDC, 0, 0, 0, FLOODFILLSURFACE);
								BLENDFUNCTION Blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
								if (GdiAlphaBlend(DestDC, 0, 0, ListRect.right, ListRect.bottom, OrigDC, 0, 47, 360, 120, Blend)) {
									ListBG = CreatePatternBrush(NewBitmap);
								}
								DeleteDC(DestDC);
							}
							DeleteObject(NewBitmap);
						}
					}
					DeleteDC(OrigDC);
				}
				ReleaseDC(ListSessions, ListDC);
			}
			DeleteObject(Bitmap);
		}
	}

	LRESULT OnCreate(HWND Parent) {
		CONST HFONT Font = GetStockFont(DEFAULT_GUI_FONT);
		CONST INT Tab = 22; // 4 = 1 character (24 looks fine, but 22 is also good on Wine)
		TCHAR LabelString[40];

		ScrollPlayed = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_SCROLLBAR, NULL,
			WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_TABSTOP | WS_GROUP | SBS_HORZ,
			0, 0, Base.x * 42, Base.y * 2,
			Parent, HMENU(IDSCROLL), NULL, NULL);
		if (!ScrollPlayed) return -1;
		PlayBar::Subclass(ScrollPlayed);

		ButtonSlowDown = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_BUTTON, _T("-"),
			WS_CHILD | WS_VISIBLE | WS_DISABLED | BS_RADIOBUTTON | BS_PUSHLIKE | BS_NOTIFY,
			Base.x * 42, 0, Base.x * 2, Base.y * 2,
			Parent, HMENU(IDSLOWDOWN), NULL, NULL);
		if (!ButtonSlowDown) return -1;

		ButtonSpeedUp = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_BUTTON, _T("+"),
			WS_CHILD | WS_VISIBLE | WS_DISABLED | BS_RADIOBUTTON | BS_PUSHLIKE | BS_NOTIFY,
			Base.x * 44, 0, Base.x * 2, Base.y * 2,
			Parent, HMENU(IDSPEEDUP), NULL, NULL);
		if (!ButtonSpeedUp) return -1;

		ListSessions = CreateWindowEx(WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE, WC_LISTBOX, NULL,
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | WS_VSCROLL | LBS_USETABSTOPS | LBS_NOINTEGRALHEIGHT | LBS_WANTKEYBOARDINPUT | LBS_NOTIFY,
			0, Base.y * 2, Base.x * 23, Base.y * 8,
			Parent, HMENU(IDLIST), NULL, NULL);
		if (!ListSessions) return -1;
		SetWindowFont(ListSessions, Font, FALSE);
		ListBox_SetTabStops(ListSessions, 1, &Tab);
		SetListBG();

		LoadString(NULL, LABEL_NO_VIDEO, LabelString, 40);
		LabelTime = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_STATIC, LabelString,
			WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_LEFTNOWORDWRAP,
			Base.x * 24, Base.y * 3, Base.x * 14, Base.y * 2,
			Parent, NULL, NULL, NULL);
		if (!LabelTime) return -1;
		SetWindowFont(LabelTime, Font, FALSE);

		StatusTime = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_STATIC, TimeStr::Time,
			WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_RIGHT,
			Base.x * 38, Base.y * 3, Base.x * 7, Base.y * 2,
			Parent, NULL, NULL, NULL);
		if (!StatusTime) return -1;
		SetWindowFont(StatusTime, Font, FALSE);

		LoadString(NULL, BUTTON_OPEN, LabelString, 40);
		ButtonSub = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_BUTTON, LabelString,
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP,
			Base.x * 24, Base.y * 6, Base.x * 10, Base.y * 3,
			Parent, HMENU(IDCANCEL), NULL, NULL);
		if (!ButtonSub) return -1;
		SetWindowFont(ButtonSub, Font, FALSE);

		LoadString(NULL, BUTTON_START, LabelString, 40);
		ButtonMain = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_BUTTON, LabelString,
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
			Base.x * 35, Base.y * 6, Base.x * 10, Base.y * 3,
			Parent, HMENU(IDOK), NULL, NULL);
		if (!ButtonMain) return -1;
		SetWindowFont(ButtonMain, Font, FALSE);

		return 0;
	}

	VOID Progress_Start() {
		if (TaskbarButton) {
			Progress = 0;
			TaskbarButton->SetProgressState(Handle, TBPF_INDETERMINATE);
		}
	}
	VOID Progress_Set(DWORD Set, DWORD End) {
		if (TaskbarButton) {
			Set += End * Progress_Segment;
			End *= Progress_Segments;
			if (End >= 100) {
				Set /= End / 100;
				End = 100;
			}
			if (Set != Progress) {
				TaskbarButton->SetProgressValue(Handle, Set, End);
				Progress = Set;
			}
		}
	}
	VOID Progress_Pause() {
		if (TaskbarButton) {
			TaskbarButton->SetProgressState(Handle, TBPF_PAUSED);
		}
	}
	VOID Progress_Error() {
		if (TaskbarButton) {
			TaskbarButton->SetProgressState(Handle, TBPF_ERROR);
		}
	}
	VOID Progress_Stop() {
		if (TaskbarButton) {
			TaskbarButton->SetProgressState(Handle, TBPF_NOPROGRESS);
		}
	}

	VOID MinimizeToTray() {
		NOTIFYICONDATA Icon;
		Icon.cbSize = NOTIFYICONDATA_V2_SIZE; //To work on Win2000 and ignore GUID
		Icon.hWnd = Handle;
		Icon.uID = IDTRAY;
		Icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_INFO | NIF_REALTIME | NIF_TIP;
		Icon.uCallbackMessage = WM_TRAY;
		Icon.hIcon = LoadSmallIcon(GetModuleHandle(NULL), Tibia::GetIcon());
		Icon.uTimeout = 10000;
		Icon.dwInfoFlags = NIIF_USER;
		if (Tibia::HostLen) {
			CopyMemory(Icon.szInfoTitle, _T("OpenTibia "), TLEN(10));
			CopyMemory(Icon.szInfoTitle + 10, Tibia::VersionString, sizeof(Tibia::VersionString));
			CopyMemoryA(Icon.szInfo, Tibia::Host, Tibia::HostLen + 1);
			CopyMemoryA(Icon.szTip, Tibia::Host, Tibia::HostLen);
			if (Tibia::HostLen < 110) {
				CopyMemory(Icon.szTip + Tibia::HostLen, _T(" - OpenTibia "), TLEN(13));
				CopyMemory(Icon.szTip + Tibia::HostLen + 13, Tibia::VersionString, sizeof(Tibia::VersionString));
			}
			else {
				CopyMemory(Icon.szTip + 106, _T("... - OpenTibia "), TLEN(16));
				CopyMemory(Icon.szTip + 122, Tibia::VersionString, sizeof(Tibia::VersionString));
			}
		}
		else if (Proxy::Port) {
			CopyMemory(Icon.szInfoTitle, _T("Tibia "), TLEN(6));
			CopyMemory(Icon.szInfoTitle + 6, Tibia::VersionString, sizeof(Tibia::VersionString));
			SIZE_T Size = LoadString(NULL, TITLE_PROXY, Icon.szInfo, 113);
			CopyMemory(Icon.szTip, Icon.szInfo, Size);
			CopyMemory(Icon.szTip + Size, _T(" - Tibia "), TLEN(9));
			CopyMemory(Icon.szTip + Size + 9, Tibia::VersionString, sizeof(Tibia::VersionString));
		}
		else {
			CopyMemory(Icon.szInfoTitle, _T("Tibia"), TLEN(6));
			CopyMemory(Icon.szInfo, Title, sizeof(Title));
			CopyMemory(Icon.szTip, Title, sizeof(Title));
		}
		if (Shell_NotifyIcon(NIM_ADD, &Icon)) {
			ShowWindow(Handle, SW_HIDE);
			EnableWindow(Handle, FALSE);
		}
	}
	VOID RemoveTray() {
		NOTIFYICONDATA Icon;
		Icon.cbSize = NOTIFYICONDATA_V2_SIZE;
		Icon.hWnd = Handle;
		Icon.uID = IDTRAY;
		Icon.uFlags = NULL;
		Shell_NotifyIcon(NIM_DELETE, &Icon);
	}
	VOID RestoreFromTray() {
		Wheel = 0;
		EnableWindow(Handle, TRUE);
		ShowWindow(Handle, SW_SHOWNORMAL);
		SetForegroundWindow(Handle);
		RemoveTray();
	}

	VOID OnClose() {
		if (Tibia::Running) {
			if (Tibia::HostLen || Proxy::Port) {
				Video::HandleTibiaClosed();
			}
			else {
				Tibia::Close();
			}
		}
		Video::SaveChanges();
		if (Tibia::Running) {
			Tibia::Flash();
			MinimizeToTray();
			if (Video::Last) {
				Video::Close();
			}
		}
		else {
			ShowWindow(Handle, SW_HIDE);
			PostQuitMessage(EXIT_SUCCESS);
			EnableWindow(Handle, FALSE);
		}
	}

	VOID OnForcedClose() {
		if (Tibia::Running) {
			Tibia::Close();
		}
		Video::SaveRecovery();
	}

	VOID OnTibiaClosed() {
		Tibia::Close();
		if (IsWindowVisible(Handle)) {
			Focus(ListSessions);
			FlashWindow(Handle, TRUE);
		}
		else {
			PostQuitMessage(EXIT_SUCCESS);
			RemoveTray();
		}
	}

	VOID OnScroll(CONST WORD Code) {
		switch (Video::State) {
		case Video::PLAY: return Video::SkipScroll(Code);
		case Video::SCROLL: return Video::ScrollRepeat(Code);
		}
	}

	VOID OnListChange() {
		switch (Video::State) {
		case Video::PLAY: return Video::SessionSelect();
		case Video::SCROLL: return Video::ScrollSessionSelect();
		}
	}

	VOID OnDelete() {
		switch (Video::State) {
		case Video::IDLE: return Video::Delete();
		case Video::WAIT: return Video::WaitDelete();
		case Video::PLAY: return Video::PlayDelete();
		case Video::SCROLL: return Video::ScrollDelete();
		}
	}

	VOID OnDeleteAll() {
		switch (Video::State) {
		case Video::IDLE:return Video::Close();
		case Video::WAIT: return Video::WaitClose();
		case Video::PLAY: return Video::PlayClose();
		case Video::SCROLL: return Video::PlayClose();
		}
	}

	VOID OnListMenu(LPARAM Pos) {
		if (Video::Last && Video::State != Video::RECORD) {
			if (HMENU DeleteMenu = CreatePopupMenu()) {
				OnScroll(SB_THUMBPOSITION);
				INT Selected;
				POINT MenuPos;
				BOOL RightMenu = GetSystemMetrics(SM_MENUDROPALIGNMENT);
				TCHAR MenuString[40];
				if (Pos == -1) { // menu hotkey or shift+F10
					if (ListBox_SetCurSel(ListSessions, Selected = ListBox_GetCurSel(ListSessions)) == LB_ERR) {
						ListBox_SetCurSel(ListSessions, Selected = 0);
						OnListChange();
					}
					RECT ItemRect;
					if (ListBox_GetItemRect(ListSessions, Selected, &ItemRect) == LB_ERR) {
						GetClientRect(ListSessions, &ItemRect);
					}
					else {
						CopyMemory(MenuString + LoadString(NULL, MENU_DELETE_SELECTED, MenuString, 32), _T("\tDelete"), TLEN(8));
						AppendMenu(DeleteMenu, MF_STRING, 1, MenuString);
					}
					MenuPos = { RightMenu ? ItemRect.right : ItemRect.left, ItemRect.bottom };
				}
				else {
					if (!GetCursorPos(&MenuPos)) {
						MenuPos = { GET_X_LPARAM(Pos), GET_Y_LPARAM(Pos) };
					}
					ScreenToClient(ListSessions, &MenuPos);
					Selected = ListBox_ItemFromPoint(ListSessions, MenuPos.x, MenuPos.y);
					if (!HIWORD(Selected)) {
						CopyMemory(MenuString + LoadString(NULL, MENU_DELETE_SELECTED, MenuString, 32), _T("\tDelete"), TLEN(8));
						AppendMenu(DeleteMenu, MF_STRING, 1, MenuString);
						if (Selected != ListBox_GetCurSel(ListSessions)) {
							ListBox_SetCurSel(ListSessions, Selected);
							OnListChange();
						}
					}
				}
				LoadString(NULL, MENU_DELETE_ALL, MenuString, 40);
				CopyMemory(MenuString + LoadString(NULL, MENU_DELETE_ALL, MenuString, 26), _T("\tShift+Delete"), TLEN(14));
				AppendMenu(DeleteMenu, MF_STRING, 2, MenuString);
				ClientToScreen(ListSessions, &MenuPos);
				RightMenu = TrackPopupMenuEx(DeleteMenu, (RightMenu ? TPM_RIGHTALIGN | TPM_HORNEGANIMATION : TPM_LEFTALIGN | TPM_HORPOSANIMATION) | TPM_TOPALIGN | TPM_VERPOSANIMATION | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, MenuPos.x, MenuPos.y, Handle, NULL);
				DestroyMenu(DeleteMenu);
				switch (RightMenu) {
					case 0: break;
					case 1: OnDelete(); break;
					case 2: return OnDeleteAll();
				}
				OnScroll(SB_ENDSCROLL);
			}
		}
	}

	VOID OnListClick() {
		if (Video::Last) switch (Video::State) {
		case Video::IDLE:
			return Video::Start();
		case Video::PLAY:
			Focus(ScrollPlayed);
			return Video::Resume();
		case Video::SCROLL:
			return Video::ScrollSessionSelect();
		}
	}

	VOID OnSlowDown() {
		Wheel = 0;
		switch (Video::State) {
		case Video::PLAY:
			Focus(ScrollPlayed);
			return Video::SlowDown();
		}
	}
	VOID OnSpeedUp() {
		Wheel = 0;
		switch (Video::State) {
		case Video::PLAY:
			Focus(ScrollPlayed);
			return Video::SpeedUp();
		}
	}

	VOID OnWheel(CONST SHORT Delta) {
		if (Video::State == Video::PLAY) {
			if (Delta < 0) {
				if (Wheel > 0) {
					Wheel = 0;
				}
				for (Wheel += Delta; Wheel <= -WHEEL_DELTA; Wheel += WHEEL_DELTA) {
					Video::SlowDown();
				}
			}
			else if (Delta > 0) {
				if (Wheel < 0) {
					Wheel = 0;
				}
				for (Wheel += Delta; Wheel >= WHEEL_DELTA; Wheel -= WHEEL_DELTA) {
					Video::SpeedUp();
				}
			}
		}
		else {
			Wheel = 0;
		}
	}

	VOID OnTimer() {
		switch (Video::State) {
		case Video::PLAY: return Video::PlayTimer();
		case Video::SCROLL: return Video::ScrollIdle();
		}
	}

	VOID OnMainButton() {
		switch (Video::State) {
		case Video::IDLE:
			Focus(ListSessions);
			return Video::Start();
		case Video::WAIT:
			Focus(ListSessions);
			return Video::Abort();
		case Video::RECORD:
			Focus(ListSessions);
			return Video::Stop();
		case Video::PLAY:
			Focus(ScrollPlayed);
			return Video::PlayPause();
		}
	}

	VOID OnSubButton() {
		switch (Video::State) {
		case Video::IDLE:
			Focus(ListSessions);
			return Video::FileDialog();
		case Video::WAIT:
			Focus(ListSessions);
			return Video::Abort();
		case Video::RECORD:
			Focus(ListSessions);
			return Video::Cancel();
		case Video::PLAY:
			Focus(ListSessions);
			return Video::Eject();
		}
	}

	VOID OnDrop(CONST HDROP Drop) {
		switch (Video::State) {
		case Video::IDLE:
			Video::OpenDrop(Drop);
			break;
		}
		DragFinish(Drop);
	}

	VOID OnTopmost() {
		if (GetWindowExStyle(Handle) & WS_EX_TOPMOST) {
			SetLayeredWindowAttributes(Handle, NULL, 255, LWA_ALPHA);
			SetWindowPos(Handle, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			CheckMenuItem(Menu, IDTOPMOST, MF_UNCHECKED);
		}
		else {
			SetLayeredWindowAttributes(Handle, NULL, 180, LWA_ALPHA);
			SetWindowPos(Handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			CheckMenuItem(Menu, IDTOPMOST, MF_CHECKED);
			if (Tibia::Running) {
				Tibia::Flash();
			}
		}
	}

	VOID OnGamemaster() {
		if (Parser->GamemasterMode = !Parser->GamemasterMode) {
			CheckMenuItem(Menu, IDGMMODE, MF_CHECKED);
		}
		else {
			CheckMenuItem(Menu, IDGMMODE, MF_UNCHECKED);
		}
	}

	VOID OnProxy() {
		if (!Tibia::Running) {
			if (Proxy::Port = !Proxy::Port) {
				CheckMenuItem(Menu, IDPROXY, MF_CHECKED);
			}
			else {
				CheckMenuItem(Menu, IDPROXY, MF_UNCHECKED);
			}
		}
	}

	VOID ShellRegister(CONST HKEY ClassKey, CONST LPCTSTR FileDescription, CONST SIZE_T DescriptionLen, CONST LPCTSTR CommandPath, CONST SIZE_T CommandLen, CONST LPCTSTR IconPath, CONST SIZE_T IconLen) {
		RegSetValueEx(ClassKey, NULL, 0, REG_SZ, LPBYTE(FileDescription), TLEN(DescriptionLen));
		HKEY ShellKey;
		if (RegCreateKeyEx(ClassKey, _T("shell"), 0, NULL, NULL, KEY_SET_VALUE | KEY_CREATE_SUB_KEY, NULL, &ShellKey, NULL) == ERROR_SUCCESS) {
			RegSetValueEx(ShellKey, NULL, 0, REG_SZ, LPBYTE(_T("open")), TLEN(4));
			HKEY CommandKey;
			if (RegCreateKeyEx(ShellKey, _T("open\\command"), 0, NULL, NULL, KEY_SET_VALUE, NULL, &CommandKey, NULL) == ERROR_SUCCESS) {
				RegSetValueEx(CommandKey, NULL, 0, REG_SZ, LPBYTE(CommandPath), TLEN(CommandLen));
				RegCloseKey(CommandKey);
			}
			RegCloseKey(ShellKey);
		}
		if (RegCreateKeyEx(ClassKey, _T("DefaultIcon"), 0, NULL, NULL, KEY_SET_VALUE, NULL, &ShellKey, NULL) == ERROR_SUCCESS) {
			RegSetValueEx(ShellKey, NULL, 0, REG_SZ, LPBYTE(IconPath), TLEN(IconLen));
			RegCloseKey(ShellKey);
		}
		RegCloseKey(ClassKey);
	}

	VOID OnRegister() {
		HKEY RootKey;
		if (RegOpenKeyEx(GetKeyState(VK_SHIFT) < 0 ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, _T("Software\\Classes"), NULL, KEY_CREATE_SUB_KEY, &RootKey) == ERROR_SUCCESS) {
			TCHAR VideoDescription[20];
			SIZE_T DescriptionLen = LoadString(NULL, FILETYPE_TTM, VideoDescription, 20);

			TCHAR IconPath[MAX_PATH + 3];
			GetModuleFileName(NULL, IconPath, MAX_PATH);
			SIZE_T IconLen = _tcslen(IconPath);

			TCHAR CommandPath[MAX_PATH + 8];
			CopyMemory(CommandPath, IconPath, TLEN(IconLen + 1));
			PathQuoteSpaces(CommandPath);
			SIZE_T CommandLen = _tcslen(CommandPath);
			CopyMemory(CommandPath + CommandLen, _T(" \"%1\""), TLEN(6));
			CommandLen += 5;

			CopyMemory(IconPath + IconLen, _T(",1"), TLEN(3));
			IconLen += 2;

			HKEY ClassKey;
			if (RegCreateKeyEx(RootKey, _T("tibia_ttm"), 0, NULL, NULL, KEY_SET_VALUE | KEY_CREATE_SUB_KEY, NULL, &ClassKey, NULL) == ERROR_SUCCESS) {
				RegSetValueEx(ClassKey, _T("InfoTip"), 0, REG_SZ, LPBYTE(_T("prop:System.FileVersion;System.Size;System.FileExtension;System.DateModified")), TLEN(76));
				ShellRegister(ClassKey, VideoDescription, DescriptionLen, CommandPath, CommandLen, IconPath, IconLen);
				if (RegCreateKeyEx(RootKey, _T(".ttm"), 0, NULL, NULL, KEY_SET_VALUE, NULL, &ClassKey, NULL) == ERROR_SUCCESS) {
					RegSetValueEx(ClassKey, NULL, 0, REG_SZ, LPBYTE(_T("tibia_ttm")), TLEN(9));
					RegCloseKey(ClassKey);
				}
				if (RegCreateKeyEx(RootKey, _T(".cam"), 0, NULL, NULL, KEY_SET_VALUE, NULL, &ClassKey, NULL) == ERROR_SUCCESS) {
					RegSetValueEx(ClassKey, NULL, 0, REG_SZ, LPBYTE(_T("tibia_ttm")), TLEN(9));
					RegCloseKey(ClassKey);
				}
				if (RegCreateKeyEx(RootKey, _T(".tmv"), 0, NULL, NULL, KEY_SET_VALUE, NULL, &ClassKey, NULL) == ERROR_SUCCESS) {
					RegSetValueEx(ClassKey, NULL, 0, REG_SZ, LPBYTE(_T("tibia_ttm")), TLEN(9));
					RegCloseKey(ClassKey);
				}
				if (RegCreateKeyEx(RootKey, _T(".rec"), 0, NULL, NULL, KEY_SET_VALUE, NULL, &ClassKey, NULL) == ERROR_SUCCESS) {
					RegSetValueEx(ClassKey, NULL, 0, REG_SZ, LPBYTE(_T("tibia_ttm")), TLEN(9));
					RegCloseKey(ClassKey);
				}
			}
			if (RegCreateKeyEx(RootKey, _T("otserv"), 0, NULL, NULL, KEY_SET_VALUE | KEY_CREATE_SUB_KEY, NULL, &ClassKey, NULL) == ERROR_SUCCESS) {
				IconPath[IconLen - 1] = '5';
				RegSetValueEx(ClassKey, _T("URL Protocol"), 0, REG_SZ, NULL, 0);
				ShellRegister(ClassKey, _T("URL:Open Tibia Server"), 21, CommandPath, CommandLen, IconPath, IconLen);
			}
			RegCloseKey(RootKey);
			SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
		}
	}

	DWORD GetAutoPlay() {
		HKEY Key;
		if (RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\Tibia"), 0, NULL, NULL, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &Key, NULL) == ERROR_SUCCESS) {
			DWORD Type;
			DWORD Delay;
			DWORD Size = 4;
			if (RegQueryValueEx(Key, _T("AutoPlay"), NULL, &Type, LPBYTE(&Delay), &Size) == ERROR_SUCCESS && Type == REG_DWORD && Size == 4) {
				RegCloseKey(Key);
				return Delay > 1000 ? 1000 : Delay;
			}
			RegCloseKey(Key);
		}
		return 0;
	}

	VOID OnAutoPlay() {
		HKEY Key;
		if (RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\Tibia"), 0, NULL, NULL, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &Key, NULL) == ERROR_SUCCESS) {
			if (RegQueryValueEx(Key, _T("AutoPlay"), NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
				if (RegDeleteValue(Key, _T("AutoPlay")) == ERROR_SUCCESS) {
					CheckMenuItem(Menu, IDAUTOPLAY, MF_UNCHECKED);
				}
			}
			else {
				DWORD Delay = 100;
				if (RegSetValueEx(Key, _T("AutoPlay"), NULL, REG_DWORD, LPBYTE(&Delay), 4) == ERROR_SUCCESS) {
					CheckMenuItem(Menu, IDAUTOPLAY, MF_CHECKED);
				}
			}
			RegCloseKey(Key);
		}
	}

	VOID OnDonate() { // URL is encrypted to prevent simple hex editing of the link (private key not on the sources)
		CONST DWORD Key[32] = { 0x000C9FF3, 0xB2CAE6D3, 0x2B6D07FE, 0xA50FBDBA, 0x4D2D8C2D, 0xCFDA29A9, 0x62BA50AF, 0x322AEAE7, 0xFFA36F9E, 0x3591DBD0, 0xB54E463F, 0xBF15D339, 0x56C43F41, 0x039F0517, 0xC5773B19, 0x2CFD7A48, 0x76130D67, 0x9009E548, 0xA8460126, 0xC9DBD96A, 0xEC1C5BD1, 0xD6A0A8A6, 0x7E52C551, 0x70308F38, 0x63378CAD, 0xA0099890, 0x1F16A4B2, 0x79988E87, 0x2CBBB348, 0xBB581867, 0xE201A67B, 0x04E9E63E };
		DWORD Link[32] = { 0x9E9FF103, 0x47D087EF, 0x3C217C1D, 0x27D628EC, 0x6CE38F04, 0x8DF0EC9E, 0x4A0791E0, 0xC33B0C06, 0x0A7C13D8, 0x022F4377, 0x6ECE2745, 0x10EE64F4, 0xE01E4D2A, 0xF7405050, 0xF0278521, 0xF694C4BF, 0x37B0AD90, 0xF4CE700B, 0x33F6BBCC, 0xC6FAE867, 0x793E3866, 0x284B5B7F, 0x7960C698, 0xF5AE3C76, 0xD0C76181, 0x315FF5F0, 0x0FEB8461, 0x5A4F2693, 0xE91AF839, 0x817A8AB3, 0x8A195F61, 0xCC79EA40 };
		BIGWORD Rsa(LPBYTE(Link), 32);
		Rsa.PowMod(65537, Key, 32);
		Rsa.Export(LPBYTE(Link), 32);
		ShellExecuteA(MainWnd::Handle, NULL, LPCSTR(Link) + 1, NULL, NULL, SW_SHOW);
	}

	VOID OnLoginClientConnect() {
		if (Proxy::State) {
			Proxy::Login.Reject();
		}
		else {
			Proxy::HandleLoginClientConnect();
		}
	}

	VOID OnGameClientConnect() {
		if (Proxy::State) {
			Proxy::Game.Reject();
		}
		else {
			Proxy::HandleGameClientConnect();
		}
	}

	VOID OnClientRead() {
		switch (Proxy::State) {
		case Proxy::LOGIN_CW:
			return Proxy::HandleOutgoingLogin();
		case Proxy::LOGIN_SC:
			Proxy::Client->Wipe();
		case Proxy::LOGIN_SW:
		case Proxy::LOGIN_UP:
			return Proxy::HandleServerClose();
		case Proxy::GAME_CWW:
			return Proxy::HandleOutgoingGameWorldname();
		case Proxy::GAME_CW:
			return Proxy::HandleOutgoingGame();
		case Proxy::GAME_SC:
		case Proxy::GAME_SWT:
			Proxy::Client->Wipe();
		case Proxy::GAME_SW:
			return Proxy::HandleServerClose();
		case Proxy::GAME_SWR:
			return Proxy::HandleOutgoingSwitch();
		case Proxy::GAME_PLAY:
			return Proxy::HandleOutgoingPlay();
		case Proxy::RECONNECT_SC:
		case Proxy::RECONNECT_SWT:
		case Proxy::RECONNECT_SW:
			return Proxy::HandleOutgoingPlayReconnect();
		case Proxy::VIDEO:
			return Proxy::HandleVideoCommand();
		}
	}

	VOID OnClientClose() {
		Proxy::Client.PrepareToClose(OnClientRead);
		switch (Proxy::State) {
		case Proxy::LOGIN_CW:
			return Proxy::HandleClientClose();
		case Proxy::LOGIN_SC:
			Proxy::Client->Wipe();
		case Proxy::LOGIN_SW:
		case Proxy::LOGIN_UP:
			return Proxy::HandleServerClose();
		case Proxy::GAME_CWW:
		case Proxy::GAME_CW:
			return Proxy::HandleClientClose();
		case Proxy::GAME_SC:
		case Proxy::GAME_SWT:
			Proxy::Client->Wipe();
		case Proxy::GAME_SW:
		case Proxy::GAME_SWR:
			return Proxy::HandleServerClose();
		case Proxy::GAME_PLAY:
			return Proxy::HandleLogout();
		case Proxy::RECONNECT_SC:
		case Proxy::RECONNECT_SWT:
		case Proxy::RECONNECT_SW:
			return Proxy::HandleReconnectClose();
		case Proxy::VIDEO:
			return Video::Logout();
		}
	}

	VOID OnServerConnect(CONST INT Error) {
		switch (Proxy::State) {
		case Proxy::LOGIN_SC:
			return Proxy::HandleLoginServerConnect(Error);
		case Proxy::GAME_SC:
			return Proxy::HandleGameServerConnect(Error);
		}
	}

	VOID OnServerRead() {
		switch (Proxy::State) {
		case Proxy::LOGIN_SW:
			return Proxy::HandleIncomingLogin();
		case Proxy::LOGIN_UP:
			return Proxy::HandleIncomingUpdate();
		case Proxy::GAME_SWT:
			return Proxy::HandleIncomingTicket();
		case Proxy::GAME_SW:
			return Proxy::HandleIncomingGame();
		case Proxy::GAME_SWR:
			return Proxy::HandleIncomingSwitch();
		case Proxy::GAME_PLAY:
			return Proxy::HandleIncomingPlay();
		case Proxy::RECONNECT_SC:
		case Proxy::RECONNECT_SWT:
		case Proxy::RECONNECT_SW:
			return Proxy::HandleIncomingPlayReconnect();
		}
	}

	VOID OnServerClose() {
		Proxy::Server.PrepareToClose(OnServerRead);
		switch (Proxy::State) {
		case Proxy::LOGIN_SC:
			Proxy::Client->Wipe();
		case Proxy::LOGIN_SW:
		case Proxy::LOGIN_UP:
			return Proxy::HandleServerClose();
		case Proxy::GAME_SC:
		case Proxy::GAME_SWT:
			Proxy::Client->Wipe();
		case Proxy::GAME_SW:
		case Proxy::GAME_SWR:
			return Proxy::HandleServerClose();
		case Proxy::GAME_PLAY:
			return Proxy::HandleLogout();
		case Proxy::RECONNECT_SC:
		case Proxy::RECONNECT_SWT:
			return Proxy::HandleReconnectClose();
		case Proxy::RECONNECT_SW:
			return Proxy::HandleReconnectSwitch();
		}
	}

	VOID OnReconnectConnect(CONST INT Error) {
		switch (Proxy::State) {
		case Proxy::RECONNECT_SC:
			return Proxy::HandleReconnectConnect(Error);
		}
	}

	VOID OnReconnectRead() {
		switch (Proxy::State) {
		case Proxy::RECONNECT_SWT:
			return Proxy::HandleIncomingReconnectTicket();
		case Proxy::RECONNECT_SW:
			return Proxy::HandleIncomingReconnect();
		}
	}

	VOID OnReconnectClose() {
		switch (Proxy::State) {
		case Proxy::RECONNECT_SC:
		case Proxy::RECONNECT_SWT:
		case Proxy::RECONNECT_SW:
			return Proxy::HandleReconnectError();
		}
	}

	LRESULT CALLBACK Procedure(HWND Handle, UINT Code, WPARAM Wp, LPARAM Lp) {
		switch (Code) {
		case WM_CREATE: return OnCreate(Handle);
		case WM_CLOSE: return OnClose(), 0;
		case WM_SYSCOLORCHANGE: {
			DeleteObject(ListBG);
			SetListBG();
		} break;
		case WM_ENDSESSION: if (GET_WM_ENDSESSION_ENDING(Wp, Lp)) {
			return OnForcedClose(), 0;
		} break;
		case WM_VKEYTOITEM: switch (GET_WM_VKEYTOITEM_ID(Wp, Lp)) {
		case IDLIST: switch (GET_WM_VKEYTOITEM_CODE(Wp, Lp)) {
		case VK_DELETE: return (GetKeyState(VK_SHIFT) < 0 ? OnDeleteAll() : OnDelete()), -2;
		} break;
		} return -1;
		case WM_CONTEXTMENU: switch (GET_WM_CONTEXTMENU_ID(Wp, Lp)) {
		case IDLIST: return OnListMenu(Lp), 0;
		} break;
		case WM_COMMAND: switch (GET_WM_COMMAND_ID(Wp, Lp)) {
		case IDOK: switch (GET_WM_COMMAND_CMD(Wp, Lp)) {
		case BN_CLICKED: return OnMainButton(), 0;
		} break;
		case IDCANCEL: switch (GET_WM_COMMAND_CMD(Wp, Lp)) {
		case BN_CLICKED: return OnSubButton(), 0;
		} break;
		case IDLIST: switch (GET_WM_COMMAND_CMD(Wp, Lp)) {
		case LBN_SELCHANGE: return OnListChange(), 0;
		case LBN_DBLCLK: return OnListClick(), 0;
		} break;
		case IDSLOWDOWN: switch (GET_WM_COMMAND_CMD(Wp, Lp)) {
		case BN_CLICKED: return OnSlowDown(), 0;
		case BN_DBLCLK: return OnSlowDown(), 0;
		} break;
		case IDSPEEDUP: switch (GET_WM_COMMAND_CMD(Wp, Lp)) {
		case BN_CLICKED: return OnSpeedUp(), 0;
		case BN_DBLCLK: return OnSpeedUp(), 0;
		} break;
		case IDTOPMOST: return OnTopmost(), 0;
		case IDGMMODE: return OnGamemaster(), 0;
		case IDPROXY: return OnProxy(), 0;
		case IDREGISTER: return OnRegister(), 0;
		case IDAUTOPLAY: return OnAutoPlay(), 0;
		case IDDONATE: return OnDonate(), 0;
		case IDVERSION: return Tibia::Flash(), 0;
		} return 0;
		case WM_TIMER: switch (GET_WM_TIMER_ID(Wp, Lp)) {
		case IDTIMER: return OnTimer(), 0;
		} return 0;
		case WM_HSCROLL: switch (GET_WM_HSCROLL_ID(Wp, Lp)) {
		case IDSCROLL: return OnScroll(GET_WM_HSCROLL_CODE(Wp, Lp)), 0;
		} return 0;
		case WM_CTLCOLORLISTBOX: if (HWND(Lp) == ListSessions && (!Video::First)) {
			return LRESULT(ListBG);
		} break;
		case WM_MOUSEWHEEL: return OnWheel(GET_WHEEL_DELTA_WPARAM(Wp)), 0;
		case WM_DROPFILES: return OnDrop(HDROP(Wp)), 0;
		case WM_TRAY: if (Tibia::Running && Wp == IDTRAY) switch (Lp) {
		case WM_LBUTTONDOWN: return Tibia::Flash(), 0;
		case WM_LBUTTONDBLCLK: return RestoreFromTray(), 0;
		} return 0;
		case WM_TIBIACLOSED: if (Tibia::Running && Wp == WPARAM(Tibia::Running) && Lp == LPARAM(Tibia::Proc.hProcess)) {
			return OnTibiaClosed(), 0;
		} return 0;
		case WM_SOCKET_LOGINSERVER: if (Tibia::Running) {
			if (Proxy::Login.Check(SOCKET(Wp))) switch (WSAGETSELECTEVENT(Lp)) {
			case FD_ACCEPT: return OnLoginClientConnect(), 0;
			}
		} return 0;
		case WM_SOCKET_GAMESERVER: if (Tibia::Running) {
			if (Proxy::Game.Check(SOCKET(Wp))) switch (WSAGETSELECTEVENT(Lp)) {
			case FD_ACCEPT: return OnGameClientConnect(), 0;
			}
		} return 0;
		case WM_SOCKET_CLIENT: if (Tibia::Running) {
			if (Proxy::Client.Check(SOCKET(Wp))) switch (WSAGETSELECTEVENT(Lp)) {
			case FD_READ: return OnClientRead(), 0;
			case FD_CLOSE: return OnClientClose(), 0;
			}
		}
		case WM_SOCKET_SERVER: if (Tibia::Running) {
			if (Proxy::Server.Check(SOCKET(Wp))) switch (WSAGETSELECTEVENT(Lp)) {
			case FD_READ: return OnServerRead(), 0;
			case FD_CONNECT: return OnServerConnect(WSAGETSELECTERROR(Lp)), 0;
			case FD_CLOSE: return OnServerClose(), 0;
			}
		} return 0;
		case WM_SOCKET_RECONNECT: if (Tibia::Running) {
			if (Proxy::Extra.Check(SOCKET(Wp))) switch (WSAGETSELECTEVENT(Lp)) {
			case FD_READ: return OnReconnectRead(), 0;
			case FD_CONNECT: return OnReconnectConnect(WSAGETSELECTERROR(Lp)), 0;
			case FD_CLOSE: return OnReconnectClose(), 0;
			}
		} return 0;
		default:
			if (Code == TaskbarRestart) {
				if (!IsWindowVisible(Handle)) {
					MinimizeToTray();
				}
				return 0;
			}
		}
		return DefDlgProc(Handle, Code, Wp, Lp);
	}

	BOOL Create(CONST LPCTSTR CmdLine, CONST INT CmdShow) {
		TaskbarRestart = RegisterWindowMessage(_T("TaskbarCreated"));

		WNDCLASSEX Class;
		Class.cbSize = sizeof(WNDCLASSEX);
		Class.style = NULL;
		Class.lpfnWndProc = Procedure;
		Class.cbClsExtra = 0;
		Class.cbWndExtra = DLGWINDOWEXTRA;
		Class.hInstance = NULL;
		Class.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
		Class.hCursor = NULL;
		Class.hbrBackground = NULL;
		Class.lpszMenuName = NULL;
		Class.lpszClassName = _T("TTMClass");
		Class.hIconSm = NULL;

		ATOM ClassAtom = RegisterClassEx(&Class);
		if (!ClassAtom) {
			return FALSE;
		}

		Class.style = CS_SAVEBITS;
		Class.lpfnWndProc = Loader::Procedure;
		Class.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(6));
		Class.lpszClassName = Loader::ClassName;
		Loader::ClassAtom = RegisterClassEx(&Class);
		if (!Loader::ClassAtom) {
			return FALSE;
		}

		HDC Screen = GetDC(NULL);
		Base.x = GetDeviceCaps(Screen, LOGPIXELSX) * 8 / 96;
		Base.y = GetDeviceCaps(Screen, LOGPIXELSY) * 8 / 96;
		ReleaseDC(NULL, Screen);

		WNDRECT Client = { 0, 0, Base.x * Width, Base.y * Height };
		AdjustWindowRect(&Client.Area, WS_CAPTION, TRUE);

		if (Menu = CreateMenu()) {
			if (HMENU OptionsMenu = CreatePopupMenu()) {
				TCHAR MenuString[40];
				LoadString(NULL, MENU_TOPMOST, MenuString, 40);
				AppendMenu(OptionsMenu, MF_STRING, IDTOPMOST, MenuString);
				LoadString(NULL, MENU_GAMEMASTER, MenuString, 40);
				AppendMenu(OptionsMenu, MF_STRING, IDGMMODE, MenuString);
				LoadString(NULL, MENU_PROXY, MenuString, 40);
				AppendMenu(OptionsMenu, MF_STRING, IDPROXY, MenuString);
				AppendMenu(OptionsMenu, MF_SEPARATOR, 0, NULL);
				LoadString(NULL, MENU_REGISTER, MenuString, 40);
				AppendMenu(OptionsMenu, MF_STRING, IDREGISTER, MenuString);
				LoadString(NULL, MENU_AUTOPLAY, MenuString, 40);
				AppendMenu(OptionsMenu, GetAutoPlay() ? MF_CHECKED : MF_STRING, IDAUTOPLAY, MenuString);
				AppendMenu(OptionsMenu, MF_SEPARATOR, 0, NULL);
				LoadString(NULL, MENU_DONATE, MenuString, 40);
				AppendMenu(OptionsMenu, MF_STRING, IDDONATE, MenuString);
				LoadString(NULL, MENU_OPTIONS, MenuString, 40);
				AppendMenu(Menu, MF_POPUP, UINT_PTR(OptionsMenu), MenuString);
			}
		}

		Handle = CreateWindowEx(WS_EX_ACCEPTFILES | WS_EX_LAYERED, LPCTSTR(ClassAtom), Title,
			WS_VISIBLE | WS_OVERLAPPED | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
			CW_USEDEFAULT, CW_USEDEFAULT, Client.Width(), Client.Height(),
			NULL, Menu, NULL, NULL);
		if (!Handle) {
			return FALSE;
		}
		SetLayeredWindowAttributes(Handle, NULL, 255, LWA_ALPHA);
		if (CmdShow == SW_SHOWMAXIMIZED) {
			ShowWindow(MainWnd::Handle, SW_SHOWNORMAL);
		}
		if (!SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, __uuidof(ITaskbarList3), (void**)&TaskbarButton))) {
			TaskbarButton = NULL;
		}
		Video::OpenCmd(CmdLine);
		Focus(ListSessions);
		return TRUE;
	}
	VOID Destroy() {
		if (TaskbarButton) {
			TaskbarButton->Release();
		}
		DestroyWindow(Handle);
	}
}
VOID ErrorBox(CONST UINT Error, CONST UINT Title) {
	MainWnd::Done();
	MainWnd::Progress_Error();
	TCHAR TitleString[50];
	LoadString(NULL, Title, TitleString, 50);
	TCHAR ErrorString[200];
	LoadString(NULL, Error, ErrorString, 200);
	MessageBox(MainWnd::Handle, ErrorString, TitleString, MB_ICONSTOP);
	MainWnd::Progress_Stop();
}

HCRYPTPROV WinCrypt;
BOOL InitFramework() {
	CONST INITCOMMONCONTROLSEX ControlsInfo = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS /*| ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES | ICC_TAB_CLASSES*/ };
	if (!InitCommonControlsEx(&ControlsInfo)) {
		InitCommonControls();
	}
	WSADATA Data;
	return SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED)) && !WSAStartup(MAKEWORD(2, 2), &Data) && CryptAcquireContext(&WinCrypt, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
}
INT APIENTRY _tWinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPTSTR CmdLine, INT CmdShow) {
	if (!InitFramework()) {
		return EXIT_FAILURE;
	}
	MSG Message;
	if (MainWnd::Create(CmdLine, CmdShow)) {
		while (GetMessage(&Message, NULL, 0, 0)) {
			if (!IsDialogMessage(MainWnd::Handle, &Message)) {
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			}
		}
		MainWnd::Destroy();
	}
	CryptReleaseContext(WinCrypt, NULL);
	WSACleanup();
	CoUninitialize();
	return (INT)Message.wParam;
}