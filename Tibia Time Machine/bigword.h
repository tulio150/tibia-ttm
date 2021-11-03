#pragma once
#include "framework.h"

struct MP_INT {
	INT Allocated;
	INT Size;
	DWORD *Ptr;
};

class BIGWORD: private MP_INT {
public:
	BIGWORD(CONST BYTE *CONST Src, CONST DWORD Size);
	BIGWORD(CONST LPCSTR Src, CONST DWORD Size);
	~BIGWORD();

	VOID Export(BYTE *CONST Dest, CONST DWORD Size);
	VOID Export(DWORD* Dest, CONST DWORD DestSize);

	VOID PowMod(CONST DWORD Exp, CONST DWORD *CONST ModData, CONST DWORD ModSize);
	VOID PowMod(CONST DWORD *CONST &ExpData, CONST DWORD ExpSize, CONST DWORD *CONST &ModData, CONST DWORD ModSize);
};