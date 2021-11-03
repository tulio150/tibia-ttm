#include "framework.h"

#define SendScroll(Handle, Code) SendMessage(GetParent((Handle)), WM_HSCROLL, (Code), LPARAM((Handle)))
#define SendWheel(Handle, Delta) SendMessage(GetParent((Handle)), WM_MOUSEWHEEL, MAKEWPARAM(0, (Delta)), -1)

namespace PlayBar {
	VOID OnLoseFocus(CONST HWND Handle) {
		if (GetWindowLongPtr(Handle, GWLP_USERDATA)) {
			SendScroll(Handle, SB_ENDSCROLL);
			SetWindowLongPtr(Handle, GWLP_USERDATA, NULL);
		}
	}
	VOID OnKeyDown(CONST HWND Handle, CONST WPARAM Key) {
		if (Key == GetWindowLongPtr(Handle, GWLP_USERDATA)){
			switch (Key) {
				case VK_PRIOR:
					SendScroll(Handle, SB_PAGELEFT);
					break;
				case VK_NEXT:
					SendScroll(Handle, SB_PAGERIGHT);
					break;
				case VK_LEFT:
					SendScroll(Handle, SB_LINELEFT);
					break;
				case VK_RIGHT:
					SendScroll(Handle, SB_LINERIGHT);
					break;
				default:
					SendScroll(Handle, SB_THUMBPOSITION);
					break;
			}
		}
		else {
			switch (Key) {
				case VK_SPACE:
					SendScroll(Handle, SB_THUMBPOSITION);
					break;
				case VK_PRIOR:
					SendScroll(Handle, SB_PAGELEFT);
					break;
				case VK_NEXT:
					SendScroll(Handle, SB_PAGERIGHT);
					break;
				case VK_END:
					SendScroll(Handle, SB_RIGHT);
					break;
				case VK_HOME:
					SendScroll(Handle, SB_LEFT);
					break;
				case VK_LEFT:
					SendScroll(Handle, SB_LINELEFT);
					break;
				case VK_UP:
					OnLoseFocus(Handle);
					SendWheel(Handle, WHEEL_DELTA);
					return;
				case VK_RIGHT:
					SendScroll(Handle, SB_LINERIGHT);
					break;
				case VK_DOWN:
					OnLoseFocus(Handle);
					SendWheel(Handle, -WHEEL_DELTA);
					return;
				default:
					return OnLoseFocus(Handle);
			}
			SetWindowLongPtr(Handle, GWLP_USERDATA, Key);
		}
	}
	VOID OnKeyUp(CONST HWND Handle, CONST WPARAM Key) {
		if (Key == GetWindowLongPtr(Handle, GWLP_USERDATA)) {
			SendScroll(Handle, SB_ENDSCROLL);
			SetWindowLongPtr(Handle, GWLP_USERDATA, NULL);
		}
	}
	LPARAM OnContextMenu(CONST HWND Handle, CONST LPARAM Pos) {
		if (Pos == -1) {
			POINT MenuPos = { 0,0 };
			ClientToScreen(Handle, &MenuPos);
			return MAKELPARAM(MenuPos.x, MenuPos.y);
		}
		return Pos;
	}

	WNDPROC OldProc;
	LRESULT CALLBACK Procedure(HWND Handle, UINT Code, WPARAM Wp, LPARAM Lp) {
		switch (Code) {
			case WM_KILLFOCUS:
				OnLoseFocus(Handle);
				break;
			case WM_CONTEXTMENU:
				Lp = OnContextMenu(Handle, Lp);
				break;
			case WM_KEYDOWN:
				OnKeyDown(Handle, Wp);
				return 0;
			case WM_KEYUP:
				OnKeyUp(Handle, Wp);
				return 0;
			case WM_LBUTTONDOWN:
				SetWindowLongPtr(Handle, GWLP_USERDATA, NULL);
				break;
		}
		return CallWindowProc(OldProc, Handle, Code, Wp, Lp);
	}
	VOID Subclass(CONST HWND Handle) {
		OldProc = SubclassWindow(Handle, Procedure);
	}
}