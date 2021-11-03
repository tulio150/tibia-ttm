#include "timestr.h"

namespace TimeStr {
	TCHAR Str[] = _T("00000\t+0000:00:00 =0000:00:00");
	CONST LPTSTR Time = Str + 19;

	VOID SetSeconds(CONST LPTSTR Dst, CONST DWORD Time) {
		DWORD Part = Time / 3600; //hours
		ListAssign(4, Split4(Dst, 0, 1, 2, 3), CharDigits(4, Part));
		Part = (Time / 60) % 60; //minutes
		ListAssign(2, Split2(Dst, 5, 6), CharDigits(2, Part));
		Part = Time % 60; //seconds
		ListAssign(2, Split2(Dst, 8, 9), CharDigits(2, Part));
	}
	VOID SetTimeSeconds(CONST DWORD Seconds) {
		SetSeconds(Time, Seconds);
	}
	VOID SetTime(CONST DWORD Miliseconds) {
		SetTimeSeconds(Miliseconds / 1000);
	}
	LPCTSTR Set(CONST INT LoginNumber, CONST DWORD Duration, CONST DWORD End) {
		SetSeconds(Str + 19, End / 1000);
		SetSeconds(Str + 7, Duration / 1000);
		if (LoginNumber < 0) {
			return Str + 5;
		}
		if (LoginNumber < 10) {
			Str[4] = DigitToChar(LoginNumber);
			return Str + 4;
		}
		if (LoginNumber < 100) {
			ListAssign(2, Split2(Str, 3, 4), CharDigits(2, LoginNumber));
			return Str + 3;
		}
		if (LoginNumber < 1000) {
			ListAssign(3, Split3(Str, 2, 3, 4), CharDigits(3, LoginNumber));
			return Str + 2;
		}
		if (LoginNumber < 10000) {
			ListAssign(4, Split4(Str, 1, 2, 3, 4), CharDigits(4, LoginNumber));
			return Str + 1;
		}
		ListAssign(5, Split5(Str, 0, 1, 2, 3, 4), CharDigits(5, LoginNumber));
		return Str;
	}
}