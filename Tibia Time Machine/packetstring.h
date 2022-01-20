#pragma once
#include "framework.h"

struct PSTRING {
	WORD Len;
	CHAR Data[];

	VOID operator =(CONST LPCSTR Src) {
		CopyMemory(Data, Src, Len);
	}
	VOID operator =(CONST LPCWSTR Src) {
		CopyMemoryW(Data, Src, Len);
	}
	BOOL operator ==(CONST LPCSTR Cmp) CONST {
		return !DiffMemory(Data, Cmp, Len);
	}

	VOID Replace(CONST LPCSTR Src, CONST WORD SrcLen) {
		CopyMemory(Data, Src, min(Len, SrcLen));
	}
	BOOL Compare(CONST LPCSTR Cmp, CONST WORD CmpLen) CONST {
		return Len == CmpLen && *this == Cmp;
	}
};
struct RSTRING {
	DWORD Len;
	LPCWSTR Data;

	RSTRING(): Len(0) {}
	RSTRING(CONST UINT ID): Len(LoadStringW(NULL, ID, LPWSTR(&Data), 0)) {}
};
struct STRING {
	WORD Len;
	LPSTR Data;

	STRING(): Len(0), Data(NULL) {}
	STRING(CONST WORD NewLen): Len(NewLen), Data(new(std::nothrow) CHAR[Len + 1]) {
		if (Data) {
			Data[Len] = 0;
		}
		else {
			Len = 0;
		}
	}
	STRING(CONST PSTRING &Src): STRING(Src.Len) {
		CopyMemory(Data, Src.Data, Len);
	}
	~STRING() {
		delete [] Data;
	}

	STRING& operator =(CONST PSTRING& Src) {
		this->~STRING();
		new(this) STRING(Src);
		return *this;
	}

	BOOL operator ==(CONST PSTRING& Cmp) {
		return Len == Cmp.Len && !DiffMemory(Data, Cmp.Data, Len);
	}
	BOOL operator !=(CONST PSTRING& Cmp) {
		return Len != Cmp.Len || DiffMemory(Data, Cmp.Data, Len);
	}

	VOID Wipe() {
		SecureZeroMemory(Data, Len);
		Len = 0;
	}
};

struct WORLD {
	BYTE ID;
	PSTRING* Name;
	PSTRING* Host;
	WORD Port;
};
struct CHARACTER {
	CHARACTER() {} //to call the string ctors

	STRING Name;
	STRING WorldName;
	STRING HostName;
	DWORD Host;
	WORD Port;
};

