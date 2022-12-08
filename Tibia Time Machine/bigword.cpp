#include "bigword.h"

//START GMP INCLUDE
//MPIR -> GMP
#define mpz_init2 __gmpz_init2
#define mpz_import __gmpz_import
#define mpz_clear __gmpz_clear
#define mpz_export __gmpz_export
#define mpz_powm __gmpz_powm
#define mpz_powm_ui __gmpz_powm_ui

extern "C" {
	VOID _cdecl mpz_init2(MP_INT *, DWORD);
	VOID _cdecl mpz_import(MP_INT *, SIZE_T, INT, SIZE_T, INT, SIZE_T, LPCVOID);
	VOID _cdecl mpz_clear(MP_INT *);
	LPVOID _cdecl mpz_export(LPVOID, SIZE_T *, INT, SIZE_T, INT, SIZE_T, CONST MP_INT *);
	VOID _cdecl mpz_powm(MP_INT *, CONST MP_INT *, CONST MP_INT *, CONST MP_INT *);
	VOID _cdecl mpz_powm_ui(MP_INT *, CONST MP_INT *, DWORD, CONST MP_INT *);
}
//END GMP INCLUDE

VOID MP_INT::Output() CONST {
	CHAR Number[9] = "00000000";
	for (INT i = 0; i < Size; i++) {
		DWORD Limb = Ptr[i];
		for (INT j = 8; j--; Limb >>= 4) {
			Number[j] = Limb & 0xF;
			Number[j] += Number[j] < 10 ? '0' : 'A' - 10;
		}
		OutputDebugStringA(", 0x");
		OutputDebugStringA(Number);
	}
	OutputDebugStringA("\n");
}

BIGWORD::BIGWORD(CONST BYTE *CONST Src) {
	mpz_init2(this, 1024);
	mpz_import(this, 32, 1, 4, 1, 0, Src);
}
BIGWORD::~BIGWORD() {
	mpz_clear(this);
}

VOID BIGWORD::Export(BYTE *CONST Dest) {
	DWORD Padding = (32 - Size) << 2;
	ZeroMemory(Dest, Padding);
	mpz_export(Dest + Padding, NULL, 1, 4, 1, 0, this);
}

VOID BIGWORD::PowMod(CONST DWORD Exp, CONST DWORD *CONST ModData) {
	CONST MP_INT Mod = {0, 32, (DWORD *) ModData};
	mpz_powm_ui(this, this, Exp, &Mod);
}
VOID BIGWORD::PowMod(CONST DWORD *CONST ExpData, CONST DWORD *CONST ModData) {
	CONST MP_INT Exp = {0, 32, (DWORD *) ExpData};
	CONST MP_INT Mod = {0, 32, (DWORD *) ModData};
	mpz_powm(this, this, &Exp, &Mod);
}
