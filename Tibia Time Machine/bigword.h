#pragma once
#include "framework.h"

struct MP_INT {
	INT Allocated;
	INT Size;
	DWORD *Ptr;

	VOID Output() CONST;
};

class BIGWORD: private MP_INT {
public:
	BIGWORD(CONST BYTE *CONST Src);
	~BIGWORD();

	VOID Export(BYTE *CONST Dest);

	VOID PowMod(CONST DWORD Exp, CONST DWORD *CONST ModData);
	VOID PowMod(CONST DWORD *CONST ExpData, CONST DWORD *CONST ModData);
};