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
struct RSTRING {
	DWORD Len;
	LPCWSTR Data;

	inline RSTRING(CONST UINT ID): Len(LoadStringW(NULL, ID, LPWSTR(&Data), 0)) {}
};
struct STRING {
	WORD Len;
	LPSTR Data;

	inline STRING(): Len(0), Data(NULL) {}
	inline STRING(CONST WORD NewLen): Len(NewLen), Data(new(nothrow) CHAR[Len + 1]) {
		if (Data) {
			Data[Len] = 0;
		}
		else {
			Len = 0;
		}
	}
	inline STRING(CONST PSTRING &Src): STRING(Src.Len) {
		CopyMemory(Data, Src.Data, Len);
	}
	inline ~STRING() {
		delete [] Data;
	}

	inline STRING& operator =(CONST PSTRING& Src) {
		this->~STRING();
		new(this) STRING(Src);
		return *this;
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

struct WORLD {
	BYTE ID;
	PSTRING* Name;
	PSTRING* Host;
	WORD Port;
};
struct CHARACTER {
	inline CHARACTER() {} //to call the string ctors

	STRING Name;
	STRING WorldName;
	STRING HostName;
	DWORD Host;
	WORD Port;
};

