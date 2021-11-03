#include "bigword.h"

//START GMP INCLUDE
//MPIR -> GMP
#define mpz_init2 __gmpz_init2
#define mpz_set_str __gmpz_set_str
#define mpz_import __gmpz_import
#define mpz_clear __gmpz_clear
#define mpz_export __gmpz_export
#define mpz_powm __gmpz_powm
#define mpz_powm_ui __gmpz_powm_ui

extern "C" {
	VOID _cdecl mpz_init2(MP_INT *, DWORD);
	INT _cdecl mpz_set_str(MP_INT *, LPCSTR, INT);
	VOID _cdecl mpz_import(MP_INT *, SIZE_T, INT, SIZE_T, INT, SIZE_T, LPCVOID);
	VOID _cdecl mpz_clear(MP_INT *);
	LPVOID _cdecl mpz_export(LPVOID, SIZE_T *, INT, SIZE_T, INT, SIZE_T, CONST MP_INT *);
	VOID _cdecl mpz_powm(MP_INT *, CONST MP_INT *, CONST MP_INT *, CONST MP_INT *);
	VOID _cdecl mpz_powm_ui(MP_INT *, CONST MP_INT *, DWORD, CONST MP_INT *);
}
//END GMP INCLUDE

BIGWORD::BIGWORD(CONST BYTE *CONST Src, CONST DWORD SrcSize) {
	mpz_init2(this, SrcSize << 5);
	mpz_import(this, SrcSize, 1, 4, 1, 0, Src);
}
BIGWORD::BIGWORD(CONST LPCSTR Src, CONST DWORD SrcSize) {
	mpz_init2(this, SrcSize << 5);
	mpz_set_str(this, Src, 10);
}
BIGWORD::~BIGWORD() {
	mpz_clear(this);
}

VOID BIGWORD::Export(BYTE *CONST Dest, CONST DWORD DestSize) {
	DWORD Padding = (DestSize - Size) << 2;
	ZeroMemory(Dest, Padding);
	mpz_export(Dest + Padding, NULL, 1, 4, 1, 0, this);
}
VOID BIGWORD::Export(DWORD *Dest, CONST DWORD DestSize) {
	CopyMemory(Dest, Ptr, Size << 2);
}

VOID BIGWORD::PowMod(CONST DWORD Exp, CONST DWORD *CONST ModData, CONST DWORD ModSize) {
	CONST MP_INT Mod = {0, (INT) ModSize, (DWORD *) ModData};
	mpz_powm_ui(this, this, Exp, &Mod);
}
VOID BIGWORD::PowMod(CONST DWORD *CONST &ExpData, CONST DWORD ExpSize, CONST DWORD *CONST &ModData, CONST DWORD ModSize) {
	CONST MP_INT Exp = {0, (INT) ExpSize, (DWORD *) ExpData};
	CONST MP_INT Mod = {0, (INT) ModSize, (DWORD *) ModData};
	mpz_powm(this, this, &Exp, &Mod);
}