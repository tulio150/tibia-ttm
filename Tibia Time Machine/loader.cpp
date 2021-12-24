#include "loader.h"
#include "main.h"
#include "tibia.h"

//IDOK = 1 // IDCANCEL = 2
#define ID_HOST		3
#define ID_PORT		4
#define ID_VERSION	5

CONST TCHAR Loader::ClassName[] = _T("TTMLoaderClass");
ATOM Loader::ClassAtom;
using MainWnd::Base;

BOOL Loader::Create(CONST HWND Parent, CONST UINT Title) {
	WNDRECT Client;
	GetWindowRect(Parent, &Client.Area);
	Client.Pos.x += (Client.Width() - Base.x * Width) / 2;
	Client.Pos.y += (Client.Height() - Base.y * Height) / 2 + GetSystemMetrics(SM_CYCAPTION);
	Client.End.x = Client.Pos.x + Base.x * Width;
	Client.End.y = Client.Pos.y + Base.y * Height;
	AdjustWindowRect(&Client.Area, WS_CAPTION, FALSE);
	Client.PreventOffscreen();

	TCHAR TitleString[100];
	LoadString(NULL, Title, TitleString, 100);

	Handle = CreateWindowEx(WS_EX_NOPARENTNOTIFY, LPCTSTR(ClassAtom), TitleString,
		WS_VISIBLE | WS_OVERLAPPED | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU,
		Client.Pos.x, Client.Pos.y, Client.Width(), Client.Height(),
		Parent, NULL, NULL, this);
	return BOOL(Handle);
}
BOOL Loader::Run(CONST HWND Parent, CONST UINT Title) {
	SetCursor(LoadCursor(NULL, IDC_WAIT));
	UpdateWindow(Parent);
	if (!Create(Parent, Title)) {
		return FALSE;
	}
	EnableWindow(Parent, FALSE);
	Focus(Title == TITLE_LOADER_OVERRIDE ? ComboVersion : ComboHost);
	SetCursor(LoadCursor(NULL, IDC_ARROW));
	MSG Message;
	while (GetMessage(&Message, NULL, 0, 0)) {
		if (!IsDialogMessage(Handle, &Message)) {
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}
	}
	EnableWindow(Parent, TRUE);
	DestroyWindow(Handle);
	return (BOOL) Message.wParam;
}
INT Loader::AddVersion(CONST WORD Version) CONST {
	Tibia::SetVersionString(Version);
	INT ComboIndex = ComboBox_FindStringExact(ComboVersion, -1, Tibia::VersionString);
	if (ComboIndex < 0) {
		for (ComboIndex = ComboBox_GetCount(ComboVersion); ComboIndex--;) {
			if (Version < ComboBox_GetItemData(ComboVersion, ComboIndex)) {
				break;
			}
		}
		ComboIndex = ComboBox_InsertString(ComboVersion, ComboIndex + 1, Tibia::VersionString);
		if (ComboIndex >= 0) {
			ComboBox_SetItemData(ComboVersion, ComboIndex, Version);
		}
	}
	return ComboIndex;
}
LRESULT Loader::OnCreate(HWND Handle) {
	SetWindowLongPtr(Handle, GWLP_USERDATA, LONG_PTR(this));

	CONST HFONT Font = GetStockFont(DEFAULT_GUI_FONT);
	TCHAR LabelString[50];
	
	LoadString(NULL, LABEL_HOST, LabelString, 50);
	LabelHost = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_STATIC, LabelString,
			WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP,
			Base.x, Base.y, Base.x * 18, Base.y * 2,
			Handle, NULL, NULL, NULL);
	if (!LabelHost) return -1;
	SetWindowFont(LabelHost, Font, FALSE);

	ComboHost = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_COMBOBOX, NULL,
			WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_AUTOHSCROLL | CBS_DROPDOWN | CBS_LOWERCASE,
			Base.x, Base.y * 3, Base.x * 18, 0,
			Handle, HMENU(ID_HOST), NULL, NULL);
	if (!ComboHost) return -1;
	SetWindowFont(ComboHost, Font, FALSE);
	ComboBox_SetMinVisible(ComboHost, 10);
	ComboBox_LimitText(ComboHost, 127);
	TCHAR Host[128];
	CopyMemoryA(Host, Tibia::Host, Tibia::HostLen + 1);
	ComboBox_SetText(ComboHost, Host);

	EditHost = ComboBox_GetEditHandle(ComboHost);
	LoadStringW(NULL, LABEL_GLOBAL, LPWSTR(LabelString), TLEN(25));
	Edit_SetCueBannerTextFocused(EditHost, LabelString, TRUE);

	LoadString(NULL, LABEL_PORT, LabelString, 50);
	LabelPort = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_STATIC, LabelString,
			WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP,
			Base.x * 20, Base.y, Base.x * 7, Base.y * 2,
			Handle, NULL, NULL, NULL);
	if (!LabelPort) return -1;
	SetWindowFont(LabelPort, Font, FALSE);

	EditPort = CreateWindowEx(WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE, WC_EDIT, NULL,
			WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_CLIPCHILDREN | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT,
			Base.x * 20, Base.y * 3, Base.x * 7, ComboBox_GetHeight(ComboHost),
			Handle, NULL, NULL, NULL);
	if (!EditPort) return -1;
	SetWindowFont(EditPort, Font, FALSE);
	Edit_LimitText(EditPort, 5);
	Edit_SetCueBannerTextFocused(EditPort, _W("0"), TRUE);

	SpinPort = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_UPDOWN, NULL,
		WS_VISIBLE | WS_CHILD | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_ARROWKEYS | UDS_NOTHOUSANDS | UDS_WRAP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		Handle, HMENU(ID_PORT), NULL, NULL);
	if (!SpinPort) return -1;
	UpDown_SetBuddy(SpinPort, EditPort);
	UpDown_SetRange(SpinPort, 1, 65535);
	UpDown_SetPos(SpinPort, Tibia::Port);

	LoadString(NULL, LABEL_VERSION, LabelString, 50);
	LabelVersion = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_STATIC, LabelString,
		WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP,
		Base.x * 28, Base.y, Base.x * 7, Base.y * 2,
		Handle, NULL, NULL, NULL);
	if (!LabelVersion) return -1;
	SetWindowFont(LabelVersion, Font, FALSE);

	ComboVersion = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_COMBOBOX, NULL,
		WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_GROUP | WS_VSCROLL | CBS_AUTOHSCROLL | CBS_DROPDOWN,
		Base.x * 28, Base.y * 3, Base.x * 7, 0,
		Handle, HMENU(ID_VERSION), NULL, NULL);
	if (!ComboVersion) return -1;
	SetWindowFont(ComboVersion, Font, FALSE);
	ComboBox_SetMinVisible(ComboVersion, 10);
	ComboBox_LimitText(ComboVersion, 5);
	Tibia::SetVersionString(Tibia::Version);
	if (ComboBox_InsertString(ComboVersion, 0, Tibia::VersionString) < 0) {
		ComboBox_SetText(ComboVersion, Tibia::VersionString);
	}
	else {
		ComboBox_SetItemData(ComboVersion, 0, Tibia::Version);
		ComboBox_SetCurSel(ComboVersion, 0);
	}

	EditVersion = ComboBox_GetEditHandle(ComboVersion);
	Edit_SetCueBannerTextFocused(EditVersion, _W("7.00+"), TRUE);

	LoadString(NULL, BUTTON_CANCEL, LabelString, 50);
	ButtonCancel = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_BUTTON, LabelString,
			WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_GROUP,
			Base.x * 14, Base.y * 7, Base.x * 10, Base.y * 3,
			Handle, HMENU(IDCANCEL), NULL, NULL);
	if (!ButtonCancel) return -1;
	SetWindowFont(ButtonCancel, Font, FALSE);

	ButtonOpen = CreateWindowEx(WS_EX_NOPARENTNOTIFY, WC_BUTTON, _T("OK"),
		WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
		Base.x * 25, Base.y * 7, Base.x * 10, Base.y * 3,
		Handle, HMENU(IDOK), NULL, NULL);
	if (!ButtonOpen) return -1;
	SetWindowFont(ButtonOpen, Font, FALSE);

	Tibia::HostLoop Loop;
	if (Loop.Start()) {
		for (DWORD Index = 0; Loop.Next(Index); Index++) {
			if (!ComboBox_InsertString(ComboHost, 0, Loop.Host)) {
				ComboBox_SetItemData(ComboHost, 0, Loop.History);
			}
			AddVersion(Loop.Version);
		}
		Loop.End();
	}

	return 0;
}
LRESULT Loader::OnClose() CONST {
	PostQuitMessage(FALSE);
	EnableWindow(Handle, FALSE);
	return 0;
}
LRESULT Loader::TooltipError(CONST HWND Control, CONST UINT Title, CONST UINT Tip, CONST UINT Fallback) CONST {
	WCHAR TitleString[50];
	WCHAR ErrorString[200];
	LoadStringW(NULL, Title, TitleString, 50);
	LoadStringW(NULL, Tip, ErrorString, 200);
	if (Edit_ShowBalloonTip2(Control, TitleString, ErrorString, TTI_ERROR)) {
		MessageBeep(MB_ICONERROR);
	}
	else {
		LoadString(NULL, TITLE_LOADER_ERROR, LPTSTR(TitleString), 50);
		LoadString(NULL, Fallback, LPTSTR(ErrorString), 200);
		MessageBox(Handle, LPTSTR(ErrorString), LPTSTR(TitleString), MB_ICONERROR);
	}
	return 0;
}
LRESULT Loader::OnConfirm() CONST {
	SIZE_T HostLen = ComboBox_GetTextLength(ComboHost);
	TCHAR Host[128];
	if (HostLen) {
		if (HostLen > 127) {
			Focus(ComboHost);
			return TooltipError(EditHost, TOOLTIP_HOST, TOOLTIP_HOST_EXPLANATION, ERROR_INVALID_HOST);
		}
		if (ComboBox_GetText(ComboHost, Host, 128) != HostLen) {
			Focus(ComboHost);
			return TooltipError(EditHost, TOOLTIP_HOST, TOOLTIP_HOST_EXPLANATION, ERROR_INVALID_HOST);
		}
		if (!Tibia::VerifyHost(Host, HostLen)) {
			Focus(ComboHost);
			return TooltipError(EditHost, TOOLTIP_HOST, TOOLTIP_HOST_EXPLANATION, ERROR_INVALID_HOST);
		}
	}
	BOOL PortError;
	WORD Port = UpDown_GetPos(SpinPort, &PortError);
	if (!Port || PortError) {
		Focus(EditPort);
		return TooltipError(EditPort, TOOLTIP_PORT, TOOLTIP_PORT_EXPLANATION, ERROR_INVALID_PORT);
	}
	WORD Version;
	INT ComboIndex = ComboBox_GetCurSel(ComboVersion);
	if (ComboIndex >= 0) {
		Version = WORD(ComboBox_GetItemData(ComboVersion, ComboIndex));
		Tibia::SetVersionString(Version);
	}
	else {
		if (ComboBox_GetText(ComboVersion, Tibia::VersionString, 6) > 5) {
			Focus(ComboVersion);
			MessageBeep(MB_ICONERROR);
			return 0;
		}
		Version = Tibia::VersionFromString();
		if (!Version || Version > LATEST) {
			Focus(ComboVersion);
			return TooltipError(EditVersion, TOOLTIP_VERSION, TOOLTIP_VERSION_EXPLANATION, ERROR_INVALID_VERSION);
		}
		ComboBox_SetText(ComboVersion, Tibia::VersionString);
	}
	Tibia::SetHost(Version, HostLen, Host, Port);
	PostQuitMessage(TRUE);
	EnableWindow(Handle, FALSE);
	return 0;
}
LRESULT Loader::OnHostSelect() CONST {
	INT ComboIndex = ComboBox_GetCurSel(ComboHost);
	if (ComboIndex >= 0) {
		DWORD History = ComboBox_GetItemData(ComboHost, ComboIndex);
		UpDown_SetPos(SpinPort, HIWORD(History));
		if (ComboBox_SetCurSel(ComboVersion, AddVersion(LOWORD(History))) < 0) {
			ComboBox_SetText(ComboVersion, Tibia::VersionString);
		}
	}
	return 0;
}
LRESULT Loader::OnHostDropDown() CONST {
	Edit_HideBalloonTip(EditHost);
	return 0;
}
LRESULT Loader::OnHostMenu(CONST LPARAM Pos) CONST {
	Focus(ComboHost);
	if (ShowMenu(MENU_REMOVE, Pos)) {
		Tibia::HostLoop Delete;
		if (ComboBox_GetText(ComboHost, Delete.Host, 128) < 128) {
			if (Delete.Start()) {
				Delete.Delete();
				Delete.End();
			}
		}
		INT ComboIndex = ComboBox_GetCurSel(ComboHost);
		if (ComboIndex < 0) {
			ComboBox_SetText(ComboHost, NULL);
			ComboIndex = ComboBox_FindStringExact(ComboHost, -1, Delete.Host);
		}
		ComboBox_DeleteString(ComboHost, ComboIndex);
	}
	return 0;
}
LRESULT Loader::OnVersionDropDown() CONST {
	Edit_HideBalloonTip(EditVersion);
	return 0;
}
LRESULT Loader::OnVersionMenu(CONST LPARAM Pos) CONST {
	Focus(ComboVersion);
	if (ShowMenu(MENU_CUSTOM, Pos)) {
		//add functionality
	}
	return 0;
}
LRESULT Loader::OnPortUpDown() CONST {
	Edit_HideBalloonTip(EditPort);
	return 0;
}
LRESULT Loader::OnPortMenu(CONST LPARAM Pos) CONST{
	Focus(EditPort);
	if (ShowMenu(MENU_PORT, Pos)) {
		UpDown_SetPos(SpinPort, PORT);
	}
	return 0;
}

BOOL Loader::ShowMenu(CONST UINT Option, CONST LPARAM Pos) CONST {
	POINT MenuPos;
	if (!GetCursorPos(&MenuPos)) {
		MenuPos = { GET_X_LPARAM(Pos), GET_Y_LPARAM(Pos) };
	}
	if (HMENU Menu = CreatePopupMenu()) {
		TCHAR MenuString[40];
		LoadString(NULL, Option, MenuString, 40);
		AppendMenu(Menu, MF_STRING, TRUE, MenuString);
		BOOL Result = TrackPopupMenu(Menu, (GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN | TPM_HORNEGANIMATION : TPM_LEFTALIGN | TPM_HORPOSANIMATION) | TPM_TOPALIGN | TPM_VERPOSANIMATION | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, MenuPos.x, MenuPos.y, 0, Handle, NULL);
		DestroyMenu(Menu);
		return Result;
	}
	return FALSE;
}

LRESULT CALLBACK Loader::Procedure(HWND Handle, UINT Code, WPARAM Wp, LPARAM Lp) {
	switch (Code) {
		case WM_CREATE: return GetLoader(Lp).OnCreate(Handle);
		case WM_CLOSE: return GetLoader(Handle).OnClose();
		case WM_CONTEXTMENU: switch (GET_WM_CONTEXTMENU_ID(Wp, Lp)) {
			case ID_HOST: return GetLoader(Handle).OnHostMenu(Lp);
			case ID_PORT: return GetLoader(Handle).OnPortMenu(Lp);
			case ID_VERSION: return GetLoader(Handle).OnVersionMenu(Lp);
		} break;
		case WM_COMMAND: switch (GET_WM_COMMAND_ID(Wp, Lp)) {
			case IDOK: switch (GET_WM_COMMAND_CMD(Wp, Lp)) {
				case BN_CLICKED: return GetLoader(Handle).OnConfirm();
			} break;
			case IDCANCEL: switch (GET_WM_COMMAND_CMD(Wp, Lp)) {
				case BN_CLICKED: return GetLoader(Handle).OnClose();
			} break;
			case ID_HOST: switch (GET_WM_COMMAND_CMD(Wp, Lp)) {
				case CBN_SELCHANGE: return GetLoader(Handle).OnHostSelect();
				case CBN_DROPDOWN: return GetLoader(Handle).OnHostDropDown();
			} break;
			case ID_VERSION: switch (GET_WM_COMMAND_CMD(Wp, Lp)) {
				case CBN_DROPDOWN: return GetLoader(Handle).OnVersionDropDown();
			} break;
		} break;
		case WM_VSCROLL: switch (GET_WM_VSCROLL_ID(Wp, Lp)) {
			case ID_PORT: return GetLoader(Handle).OnPortUpDown();
		} break;
	}
	return DefDlgProc(Handle, Code, Wp, Lp);
}