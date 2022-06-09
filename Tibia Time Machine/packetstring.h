#pragma once
#include "framework.h"

struct PSTRING {
	WORD Len;
	CHAR Data[];

	inline VOID operator =(CONST LPCSTR Src) {
		CopyMemory(Data, Src, Len);
	}
	inline VOID operator =(CONST LPCWSTR Src) {
		CopyMemoryW(Data, Src, Len);
	}
	inline BOOL operator ==(CONST LPCSTR Cmp) CONST {
		return !DiffMemory(Data, Cmp, Len);
	}

	inline VOID Replace(CONST LPCSTR Src, CONST WORD SrcLen) {
		CopyMemory(Data, Src, min(Len, SrcLen));
	}
	inline BOOL Compare(CONST LPCSTR Cmp, CONST WORD CmpLen) CONST {
		return Len == CmpLen && *this == Cmp;
	}
};
struct TSTRING {
	WORD Len;
	LPSTR Data;

	inline TSTRING(): Len(0), Data(NULL) {}
	inline TSTRING(CONST WORD NewLen): Len(NewLen), Data(new(nothrow) CHAR[NewLen + 1]) {
		if (Data) {
			Data[Len] = 0;
		}
		else {
			Len = 0;
		}
	}
	inline TSTRING(CONST PSTRING &Src): TSTRING(Src.Len) {
		CopyMemory(Data, Src.Data, Len);
	}
	inline ~TSTRING() {
		delete [] Data;
	}

	inline TSTRING& operator =(CONST PSTRING& Src) {
		this->~TSTRING();
		return *new(this) TSTRING(Src);
	}

	inline BOOL operator ==(CONST PSTRING& Cmp) {
		return Cmp.Compare(Data, Len);
	}
	inline BOOL operator !=(CONST PSTRING& Cmp) {
		return !Cmp.Compare(Data, Len);
	}

	inline VOID Wipe() {
		SecureZeroMemory(Data, Len);
		Len = 0;
	}
};
struct RSTRING {
	DWORD Len;
	LPCWSTR Data;

	inline RSTRING(CONST UINT ID): Len(LoadStringW(NULL, ID, LPWSTR(&Data), 0)) {}
};

struct CHARACTER {
	inline CHARACTER() {} //to call the string ctors

	TSTRING Name;
	TSTRING WorldName;
	TSTRING HostName;
	DWORD Host;
	WORD Port;
};
struct WORLD {
	BYTE ID;
	PSTRING* Name;
	PSTRING* Host;
	WORD Port;
};

