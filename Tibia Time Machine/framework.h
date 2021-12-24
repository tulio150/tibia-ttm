#pragma once

#pragma warning(disable:4200) //zero-sized array
#pragma warning(disable:4244) //possible loss of data
#pragma warning(disable:4355) //this on initializer list
#pragma warning(disable:4996) //GetVersion

#define _WIN32_WINNT _WIN32_WINNT_WINXP
#define NTDDI_VERSION NTDDI_WINXPSP2
#define WIN32_LEAN_AND_MEAN
#include <SDKDDKVer.h>
// Arquivos de Cabeçalho do Windows
#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dlgs.h>
#include <shlwapi.h>
#include <Shellapi.h>
#include <shlobj.h>
#include <winsock2.h>
#include <mmsystem.h>
#include <windowsx.h>
#include <process.h>
#include <wincrypt.h>
#include <new.h>

typedef unsigned __int64 QWORD;

//infinite loop
#define ever ;;

//count of array itens
#define countof(a) (sizeof(a) / sizeof(a[0]))

//WSTR
#define _W(x) L ## x

//len of TSTR in bytes
#define TLEN(x) ((x) * sizeof(TCHAR))

//A to T (len in src bytes)
inline VOID CopyMemoryA(CONST LPTSTR Dst, CONST LPCSTR Src, SIZE_T Len) { 
	while (Len--) {
		Dst[Len] = Src[Len];
	}
}
//W to A (len in dest bytes)
inline VOID CopyMemoryW(CONST LPSTR Dst, CONST LPCWSTR Src, SIZE_T Len) {
	while (Len--) {
		Dst[Len] = Src[Len];
	}
}
//Memory comparison
#define DiffMemory memcmp

#ifdef _DEBUG
#define Debug(x) OutputDebugStringA(x "\n")
#else
#define Debug(x)
#endif

#define NotLowLetter(c) ((c) < 'a' || (c) > 'z')

//arguments -> list = l, i, s, t
#define List2(l2, l1) (l2), (l1)
#define List3(l3, l2, l1) (l3), List2(l2, l1)
#define List4(l4, l3, l2, l1) (l4), List3(l3, l2, l1)
#define List5(l5, l4, l3, l2, l1) (l5), List4(l4, l3, l2, l1)

//assignment between arguments => list = (l = s), (i = t)
#define ListAssign2(l2, l1, v2, v1)							((l2) = (v2)), ((l1) = (v1))
#define ListAssign3(l3, l2, l1, v3, v2, v1)					((l3) = (v3)), ListAssign2(l2, l1, v2, v1)
#define ListAssign4(l4, l3, l2, l1, v4, v3, v2, v1)			((l4) = (v4)), ListAssign3(l3, l2, l1, v3, v2, v1)
#define ListAssign5(l5, l4, l3, l2, l1, v5, v4, v3, v2, v1)	((l5) = (v5)), ListAssign4(l4, l3, l2, l1, v4, v3, v2, v1)

//function in every argument => list = f(l), f(i), f(s), f(t)
#define ListFunction2(l2, l1, f)				f(l2), f(l1)
#define ListFunction3(l3, l2, l1, f)			f(l3), ListFunction2(l2, l1, f)
#define ListFunction4(l4, l3, l2, l1, f)		f(l4), ListFunction3(l3, l2, l1, f)
#define ListFunction5(l5, l4, l3, l2, l1, f)	f(l5), ListFunction4(l4, l3, l2, l1, f)

//operation between arguments => result = (f(l) op f(i) op f(s) op f(t))
#define ListOperation2(l2, l1, op, f)				(f(l2) op f(l1))
#define ListOperation3(l3, l2, l1, op, f)			(f(l3) op ListOperation2(l2, l1, op, f))
#define ListOperation4(l4, l3, l2, l1, op, f)		(f(l4) op ListOperation3(l3, l2, l1, op, f))
#define ListOperation5(l5, l4, l3, l2, l1, op, f)	(f(l5) op ListOperation4(l4, l3, l2, l1, op, f))

//array -> list => list = a[l], a[i], a[s], a[t]
#define Split2(a, a2, a1)				((a)[(a2)]), ((a)[(a1)])
#define Split3(a, a3, a2, a1)			((a)[(a3)]), Split2(a, a2, a1)
#define Split4(a, a4, a3, a2, a1)		((a)[(a4)]), Split3(a, a3, a2, a1)
#define Split5(a, a5, a4, a3, a2, a1)	((a)[(a5)]), Split4(a, a4, a3, a2, a1)

//list -> arguments Op(4, list) = Op4(l, i, s, t)
#define ExpandLists(var_args_func) var_args_func
#define ListAssign(x, list, values)		ExpandLists(ListAssign##x(list, values))
#define ListFunction(x, list, f)		ExpandLists(ListFunction##x(list, f))
#define ListOperation(x, list, op, f)	ExpandLists(ListOperation##x(list, op, f))
#define Split(x, a, indexes)			ExpandLists(Split##x((a), indexes))

//digit <-> char
#define IsCharDigit(c)	((c) >= '0' && (c) <= '9')
#define CharToDigit(c)	((c) - '0')
#define IsDigit(d)		((d) < 10)
#define DigitToChar(d)	((CHAR)((d) + '0'))

//list of digits <-> list of chars
#define AreCharDigits(x, chars)		ListOperation(x, chars, ||, IsCharDigit)
#define CharsToDigits(x, chars)		ListFunction(x, chars, CharToDigit)
#define AreDigits(x, digits)		ListOperation(x, digits, ||, IsDigit)
#define DigitsToChars(x, digits)	ListFunction(x, digits, DigitToChar)

//decimal <-> list of digits
#define Digit1(i) ((i) % 10)
#define Digit2(i) Digit1((i) / 10)
#define Digit3(i) Digit1((i) / 100)
#define Digit4(i) Digit1((i) / 1000)
#define Digit5(i) Digit1((i) / 10000)

#define Digits2(i) Digit2(i), Digit1(i)
#define Digits3(i) Digit3(i), Digits2(i)
#define Digits4(i) Digit4(i), Digits3(i)
#define Digits5(i) Digit5(i), Digits4(i)
#define Digits(x, i) Digits##x(i)

#define Decimal2(d2, d1)				((d2) * 10		+ (d1))
#define Decimal3(d3, d2, d1)			((d3) * 100		+ Decimal2(d2, d1))
#define Decimal4(d4, d3, d2, d1)		((d4) * 1000	+ Decimal3(d3, d2, d1))
#define Decimal5(d5, d4, d3, d2, d1)	((d5) * 10000	+ Decimal4(d4, d3, d2, d1))
#define Decimal(x, digits) ExpandLists(Decimal##x(digits))

//decimal <-> list of chars
#define CharDigit(x, i)			DigitToChar(Digit##x(i))
#define CharDigits(x, i)		DigitsToChars(x, Digits(x, i))
#define DecimalChars(x, chars)	Decimal(x, CharsToDigits(x, List##x(chars)))

#include "lang.h"
