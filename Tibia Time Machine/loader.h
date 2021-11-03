#pragma once
#include "framework.h"

class Loader {
	static CONST INT Width = 36;
	static CONST INT Height = 11;
	static Loader &GetLoader(CONST LPARAM Lp) {
		return *(Loader *) LPCREATESTRUCT(Lp)->lpCreateParams;
	}
	static Loader &GetLoader(CONST HWND Handle) {
		return *(Loader *) GetWindowLongPtr(Handle, GWLP_USERDATA);
	}

	HWND Handle;

	HWND LabelVersion;
	HWND ComboVersion;
	HWND EditVersion;

	HWND LabelHost;
	HWND ComboHost;
	HWND EditHost;

	HWND LabelPort;
	HWND EditPort;
	HWND SpinPort;

	HWND ButtonCancel;
	HWND ButtonOpen;

	BOOL Create(CONST HWND Parent, CONST UINT Title);

	LRESULT OnCreate(CONST HWND Handle);
	LRESULT OnClose() CONST;
	LRESULT OnConfirm() CONST;
	LRESULT OnHostSelect() CONST;
	LRESULT OnHostDropDown() CONST;
	LRESULT OnHostMenu(CONST LPARAM Pos) CONST;
	LRESULT OnPortUpDown() CONST;
	LRESULT OnPortMenu(CONST LPARAM Pos) CONST;
	LRESULT OnVersionDropDown() CONST;
	LRESULT OnVersionMenu(CONST LPARAM Pos) CONST;

	INT AddVersion(CONST WORD Version) CONST;
	LRESULT TooltipError(CONST HWND Control, CONST UINT Title, CONST UINT Tip, CONST UINT Fallback) CONST;
	BOOL ShowMenu(CONST UINT Option, CONST LPARAM Pos) CONST;
	VOID Focus(CONST HWND Target) CONST {
		DefDlgProc(Handle, WM_NEXTDLGCTL, WPARAM(Target), TRUE);
	}
public:
	static CONST TCHAR ClassName[];
	static ATOM ClassAtom;
	static LRESULT CALLBACK Procedure(HWND Handle, UINT Code, WPARAM Wp, LPARAM Lp);

	BOOL Run(CONST HWND Parent, CONST UINT Title);
};