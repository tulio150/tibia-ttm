#pragma once
#include "framework.h"

struct PacketData {
	WORD Size;
	BYTE Data[];

	DWORD RawSize() CONST {
		return DWORD(Size) + 2;
	}
	
	VOID Copy(CONST LPBYTE Src) {
		CopyMemory(Data, Src, Size);
	}
	VOID Wipe() {
		SecureZeroMemory(Data, Size);
	}
};

class PacketBase {
protected:
	PacketData* P;

public:
	PacketBase() : P(NULL) {}
	PacketData* operator->() CONST { return P; }
	PacketData* operator&() CONST { return P; }
};
