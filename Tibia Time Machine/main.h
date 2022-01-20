#pragma once
#include "framework.h"

//icon macros
#define LoadSmallIcon(hinst, res) ((HICON)LoadImage((HINSTANCE)(hinst), (LPCTSTR)(res), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED))
#define SetWindowIcon(hwnd, type, icon) ((HICON)SNDMSG((hwnd), WM_SETICON, (type), (LPARAM)(icon)))

//parameter translation macros
#define GET_WM_ENDSESSION_ENDING(wp, lp) ((BOOL)(wp))
#define GET_WM_SYSCOMMAND_CMD(wp, lp) ((wp) & 0xFFF0)
#define GET_WM_SYSCOMMAND_FLAGS(wp, lp) ((wp) & 0xF)
#define GET_WM_TIMER_ID(wp, lp) ((UINT_PTR)(wp))

//macros to get control ID for messages that have only HWND
#define GET_WM_VKEYTOITEM_ID(wp, lp) GetDlgCtrlID((HWND)(lp))
#define GET_WM_CONTEXTMENU_ID(wp, lp) GetDlgCtrlID((HWND)(wp))
#define GET_WM_HSCROLL_ID(wp, lp) GetDlgCtrlID((HWND)(lp))
#define GET_WM_VSCROLL_ID(wp, lp) GetDlgCtrlID((HWND)(lp))

//button set icon (not on XP)
#define Button_SetIcon(hwndCtl, icon) ((void)SNDMSG((hwndCtl), BM_SETIMAGE, IMAGE_ICON, (LPARAM)(icon)))

//better scrollbar support for this case
#undef ScrollBar_Enable
#define ScrollBar_Enable(hwnd, fEnable) EnableWindow((hwnd), (fEnable))
#define ScrollBar_SetInfo(hwnd, lpsi, fRedraw) SetScrollInfo((hwnd), SB_CTL, (lpsi), (fRedraw))
#define ScrollBar_GetInfo(hwnd, lpsi) GetScrollInfo((hwnd), SB_CTL, (lpsi))

//updown support
#define UpDown_Enable(hwnd, fEnable) EnableWindow((hwnd), (fEnable))
#define UpDown_GetPos(hwnd, pfError) ((INT)SNDMSG((hwnd), UDM_GETPOS32, 0L, (LPARAM)(pfError)))
#define UpDown_SetBuddy(hwnd, buddy) ((HWND)SNDMSG((hwnd), UDM_SETBUDDY, (WPARAM)(buddy), 0L))
#define UpDown_SetPos(hwnd, pos) ((INT)SNDMSG((hwnd), UDM_SETPOS32, 0L, (LPARAM)(pos)))
#define UpDown_SetRange(hwnd, min, max) ((VOID)SNDMSG((hwnd), UDM_SETRANGE32, (WPARAM)(min), (LPARAM)(max)))
#define WC_UPDOWN UPDOWN_CLASS

//listbox support
#define ListBox_ItemFromPoint(hwnd, x, y) ((INT)SNDMSG((hwnd), LB_ITEMFROMPOINT, 0L, MAKELPARAM((x), (y))))

//combobox inlines
inline INT ComboBox_GetHeight(CONST HWND hwnd) {
	RECT height;
	return GetClientRect(hwnd, &height) ? height.bottom : 0;
}
inline HWND ComboBox_GetEditHandle(CONST HWND hwnd) {
	COMBOBOXINFO info = { sizeof(COMBOBOXINFO) };
	return GetComboBoxInfo(hwnd, &info) ? info.hwndItem : NULL;
}

//edit inlines
inline BOOL Edit_ShowBalloonTip2(CONST HWND hwnd, CONST LPCWSTR pszwTitle, CONST LPCWSTR pszwText, CONST INT ttiIcon) {
	CONST EDITBALLOONTIP sTip = { sizeof(EDITBALLOONTIP), pszwTitle, pszwText, ttiIcon };
	return Edit_ShowBalloonTip(hwnd, &sTip);
}

//Vsta+ Balloon tip flags
#define NIF_REALTIME    0x00000040
#define NIIF_LARGE_ICON 0x00000020

//IDOK = 1 // IDCANCEL = 2
#define IDLIST		3
#define IDSCROLL	4
#define IDSLOWDOWN	5
#define IDSPEEDUP	6
//menus
#define TOP_OPTIONS	0
#define IDTOPMOST	7
#define IDGMMODE	8
#define IDPROXY		9
#define IDREGISTER	10
#define IDAUTOPLAY	11
#define IDDONATE	12
#define TOP_VERSION	1
#define IDVERSION	13

#define IDTIMER 1
#define IDTRAY	1

#define WM_TRAY					WM_USER + 100
#define WM_TIBIACLOSED			WM_USER + 101
#define WM_SOCKET_LOGINSERVER	WM_USER + 102
#define WM_SOCKET_GAMESERVER	WM_USER + 103
#define WM_SOCKET_CLIENT		WM_USER + 104
#define WM_SOCKET_SERVER		WM_USER + 105
#define WM_SOCKET_RECONNECT		WM_USER + 106

union WNDRECT {
	RECT Area;
	struct {
		POINT Pos;
		POINT End;
	};
	LONG Width() { return End.x - Pos.x; }
	LONG Height() { return End.y - Pos.y; }
	VOID PreventOffscreen() {
		INT Limit = GetSystemMetrics(SM_XVIRTUALSCREEN);
		if (Pos.x < Limit) {
			End.x -= Pos.x - Limit;
			Pos.x = Limit;
		}
		Limit += GetSystemMetrics(SM_CXVIRTUALSCREEN);
		if (End.x > Limit) {
			Pos.x -= End.x - Limit;
			End.x = Limit;
		}
		Limit = GetSystemMetrics(SM_YVIRTUALSCREEN);
		if (Pos.y < Limit) {
			End.y -= Pos.y - Limit;
			Pos.y = Limit;
		}
		Limit += GetSystemMetrics(SM_CYVIRTUALSCREEN);
		if (End.y > Limit) {
			Pos.y -= End.y - Limit;
			End.y = Limit;
		}
	}
};

namespace MainWnd {
	CONST TCHAR Title[] = _T("Tibia Time Machine");
	CONST INT Width = 46;
	CONST INT Height = 10;

	extern DWORD Progress_Segment;
	extern DWORD Progress_Segments;

	extern POINT Base;
	extern HWND Handle;

	extern HWND ScrollPlayed;
	extern HWND ButtonSlowDown;
	extern HWND ButtonSpeedUp;
	extern HWND ListSessions;
	extern HWND LabelTime;
	extern HWND StatusTime;
	extern HWND ButtonSub;
	extern HWND ButtonMain;

	extern HMENU Menu;

	extern SCROLLINFO ScrollInfo;

	inline 	VOID Focus(CONST HWND Target) {
		DefDlgProc(Handle, WM_NEXTDLGCTL, WPARAM(Target), TRUE);
	}
	inline VOID Wait() {
		SetCursor(LoadCursor(NULL, IDC_WAIT));
		UpdateWindow(Handle);
	}
	inline VOID Done() {
		SetCursor(LoadCursor(NULL, IDC_ARROW));
	}

	VOID Progress_Start();
	VOID Progress_Set(DWORD Set, DWORD End);
	VOID Progress_Pause();
	VOID Progress_Error();
	VOID Progress_Stop();

	DWORD GetAutoPlay();
	VOID MinimizeToTray();
}

namespace PlayBar {
	VOID Subclass(CONST HWND Handle);
}

VOID ErrorBox(CONST UINT Error, CONST UINT Title);

extern HCRYPTPROV WinCrypt;
