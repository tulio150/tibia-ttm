#pragma once
#include "framework.h"

namespace TimeStr {
	extern CONST LPTSTR Time;

	VOID SetTimeSeconds(CONST DWORD Seconds);
	VOID SetTime(CONST DWORD Miliseconds);

	LPCTSTR Set(CONST INT LoginNumber, CONST DWORD Duration, CONST DWORD End);
}