#pragma once
#include "framework.h"
#include "zlib.h"

class File { // generic file read/write
protected:
	HANDLE Handle;

	VOID Delete(CONST LPCTSTR FileName) {
		HANDLE(WINAPI * ReOpenFile)(HANDLE, DWORD, DWORD, DWORD) = (HANDLE(WINAPI*)(HANDLE, DWORD, DWORD, DWORD)) GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "ReOpenFile");
		if (!CloseHandle(ReOpenFile ? ReOpenFile(Handle, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_FLAG_DELETE_ON_CLOSE) : CreateFile(FileName, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL))) {
			DeleteFile(FileName);
		}
	}

public:
	File(): Handle(INVALID_HANDLE_VALUE) {}
	~File() {
		CloseHandle(Handle);
	}

	BOOL Open(CONST LPCTSTR FileName) {
		return (Handle = CreateFile(FileName, FILE_READ_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) != INVALID_HANDLE_VALUE;
	}
	DWORD GetSize() CONST {
		LARGE_INTEGER Size;
		return GetFileSizeEx(Handle, &Size) ? Size.HighPart ? INVALID_FILE_SIZE : Size.LowPart : 0;
	}
	BOOL Skip(CONST DWORD Size) CONST {
		LARGE_INTEGER Position = { Size, 0 };
		return SetFilePointerEx(Handle, Position, NULL, FILE_CURRENT);
	}
	BOOL Read(CONST LPVOID Dest, CONST DWORD Size) CONST {
		DWORD Read;
		return ReadFile(Handle, Dest, Size, &Read, NULL) && Read == Size;
	}
	BOOL ReadByte(BYTE &Dest) CONST {
		return Read(&Dest, 1);
	}
	BOOL ReadWord(WORD &Dest) CONST {
		return Read(&Dest, 2);
	}
	BOOL ReadDword(DWORD &Dest) CONST {
		return Read(&Dest, 4);
	}

	BOOL Create(CONST LPCTSTR FileName) {
		return (Handle = CreateFile(FileName, FILE_READ_DATA | FILE_WRITE_DATA | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) != INVALID_HANDLE_VALUE;
	}
	BOOL Append(CONST LPCTSTR FileName) {
		if ((Handle = CreateFile(FileName, FILE_WRITE_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) != INVALID_HANDLE_VALUE) {
			SetFilePointer(Handle, 0, NULL, FILE_END);
			return TRUE;
		}
		return FALSE;
	}
	BOOL Write(CONST LPCVOID Src, CONST DWORD Size) CONST {
		DWORD Written;
		return WriteFile(Handle, Src, Size, &Written, NULL) && Written == Size;
	}
	BOOL WriteByte(CONST BYTE Src) CONST {
		return Write(&Src, 1);
	}
	BOOL WriteWord(CONST WORD Src) CONST {
		return Write(&Src, 2);
	}
	BOOL WriteDword(CONST DWORD Src) CONST {
		return Write(&Src, 4);
	}
};

class MappedFile : protected File { // uses memory mapping to speed up read/write, fails if not enough memory
	HANDLE Map;

protected:
	LPBYTE Ptr;
	LPBYTE Data;
	LPBYTE End;

public:
	MappedFile(): Map(NULL), Ptr(NULL) {}
	~MappedFile() {
		UnmapViewOfFile(Ptr);
		CloseHandle(Map);
	}

	BOOL Open(CONST LPCTSTR FileName) {
		if (File::Open(FileName)) {
			if (DWORD Size = GetSize()) {
				if (Map = CreateFileMapping(Handle, NULL, PAGE_READONLY, NULL, Size, NULL)) {
					if (Ptr = LPBYTE(MapViewOfFile(Map, FILE_MAP_READ, NULL, NULL, Size))) {
						return BOOL(End = (Data = Ptr) + Size);
					}
				}
			}
		}
		return FALSE;
	}
	LPBYTE Skip(CONST DWORD Size) {
		LPBYTE Result = Data;
		return (Data += Size) <= End ? Result : NULL;
	}
	BOOL Read(CONST LPVOID Dest, CONST DWORD Size) {
		if (LPBYTE Src = Skip(Size)) {
			return BOOL(CopyMemory(Dest, Src, Size));
		}
		return FALSE;
	}
	BOOL ReadByte(BYTE& Dest) {
		return Read(&Dest, 1);
	}
	BOOL ReadWord(WORD& Dest) {
		return Read(&Dest, 2);
	}
	BOOL ReadDword(DWORD& Dest) {
		return Read(&Dest, 4);
	}

	BOOL Create(CONST LPCTSTR FileName, CONST DWORD Size) {
		if (File::Create(FileName)) {
			if (Map = CreateFileMapping(Handle, NULL, PAGE_READWRITE, NULL, Size, NULL)) {
				if (Ptr = LPBYTE(MapViewOfFile(Map, FILE_MAP_WRITE, NULL, NULL, Size))) {
					return BOOL(Data = Ptr);
				}
			}
			Delete(FileName);
		}
		return FALSE;
	}
	BOOL Save(CONST LPCTSTR FileName) {
		if (!FlushViewOfFile(Ptr, Data - Ptr)) {
			Delete(FileName);
			return FALSE;
		}
		return TRUE;
	}
	VOID Write(CONST LPCVOID Src, CONST DWORD Size) {
		CopyMemory(Data, Src, Size);
		Data += Size;
	}
	VOID WriteByte(CONST BYTE Src) {
		*Data++ = Src;
	}
	VOID WriteWord(CONST WORD Src) {
		*(*(LPWORD*)&Data)++ = Src;
	}
	VOID WriteDword(CONST DWORD Src) {
		*(*(LPDWORD*)&Data)++ = Src;
	}
};

class BufferedFile : protected File { // uses a stack buffer to speed up writes, does not need file size in advance
protected:
	LPCTSTR DeleteName;
	DWORD Pos;
	BYTE Buffer[0x20000];

public:
	BufferedFile(): Pos(0), DeleteName(NULL) {}

	BOOL Create(CONST LPCTSTR FileName) {
		return File::Create(DeleteName = FileName);
	}
	BOOL Save() {
		if (Pos && !File::Write(Buffer, Pos)) {
			Delete(DeleteName);
			return FALSE;
		}
		return TRUE;
	}
	BOOL Write(CONST LPCVOID Src, CONST DWORD Size) {
		if (Size <= (sizeof(Buffer) - Pos)) {
			CopyMemory(Buffer + Pos, Src, Size);
			Pos += Size;
		}
		else {
			if (Pos && !File::Write(Buffer, Pos)) {
				Delete(DeleteName);
				return FALSE;
			}
			if (Size < sizeof(Buffer)) {
				CopyMemory(Buffer, Src, Pos = Size);
			}
			else {
				if (!File::Write(Src, Size)) {
					Delete(DeleteName);
					return FALSE;
				}
				Pos = 0;
			}
		}
		return TRUE;
	}
	BOOL WriteByte(CONST BYTE Src) {
		return Write(&Src, 1);
	}
	BOOL WriteWord(CONST WORD Src) {
		return Write(&Src, 2);
	}
	BOOL WriteDword(CONST DWORD Src) {
		return Write(&Src, 4);
	}
};

#define SZ_ERROR_DATA 1
extern "C" { // Modded LzmaLib for compression progress
	// *PropsSize must be = 5 // 0 <= Level <= 9, default = 5 // DictSize = 0, default to (1 << 24) // 0 <= lc <= 8, default = 3 // 0 <= lp <= 4, default = 0 // 0 <= pb <= 4, default = 2 // 5 <= fb <= 273, default = 32 // NumThreads = 1 or 2, default = 2
	INT __stdcall LzmaCompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD SrcLen, BYTE* Props, DWORD PropsSize, INT Level, DWORD DictSize, INT lc, INT lp, INT pb, INT fb, INT Threads, LPVOID Callback);
	INT __stdcall LzmaUncompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD* SrcLen, CONST BYTE* Props, DWORD PropsSize);
}

class LzmaFile : public MappedFile { // saving is still slow and uses a lot of memory
	LPBYTE Buf;

public:
	LzmaFile() : Buf(NULL) {}
	~LzmaFile() {
		delete[] Buf;
	}

	BOOL Create(CONST DWORD Size, DWORD Header) {
		return (Buf = new(std::nothrow) BYTE[Size + (Header += Size + 17)]) ? BOOL(End = (Data = Buf) + Header): FALSE;
	}
	VOID Compress() {
		Data = End;
	}
	BOOL Save(CONST LPCTSTR FileName, CONST LPVOID Callback) {
		DWORD Size = Data - End;
		Data = End - Size - 8;
		WriteDword(Size);
		WriteDword(0);
		if (!LzmaCompress(Data, &Size, End, Size, Data - 13, 5, 5, 0, 3, 0, 2, 32, 4, Callback)) {
			*(DWORD*)(Data - 17) = Size + 13;
			if (File::Create(FileName)) {
				if (File::Write(Buf, Data - Buf + Size)) {
					return TRUE;
				}
				Delete(FileName);
			}
		}
		return FALSE;
	}
	BOOL Uncompress(CONST BOOL AllowTruncated) {
		DWORD OldSize;
		if (ReadDword(OldSize) && (Data + OldSize <= End || (AllowTruncated && (OldSize = End - Data)))) {
			if (CONST LPBYTE Props = Skip(5)) {
				DWORD Size, Large;
				if (ReadDword(Size) && Size && ReadDword(Large) && !Large) {
					if (Buf = new(std::nothrow) BYTE[Size]) {
						if (LzmaUncompress(Buf, &Size, Data, &(OldSize -= 13), Props, 5) != SZ_ERROR_DATA) {
							return BOOL(End = (Data = Buf) + Size);
						}
					}
				}
			}
		}
		return FALSE;
	}
};

class GzrFile : private MappedFile, z_stream { // fastest possible gzip decompression
public:
	GzrFile() {
		zalloc = Z_NULL;
		zfree = Z_NULL;
	}
	~GzrFile() {
		inflateEnd(this);
	}

	BOOL Open(CONST LPCTSTR FileName) {
		if (MappedFile::Open(FileName)) {
			next_in = Data;
			avail_in = End - Data;
			return inflateInit2(this, MAX_WBITS + 16) == Z_OK;
		}
		return FALSE;
	}
	BOOL Read(CONST LPVOID Dest, CONST DWORD Size) {
		next_out = LPBYTE(Dest);
		avail_out = Size;
		return inflate(this, Z_SYNC_FLUSH) >= Z_OK && !avail_out;
	}
	BOOL ReadByte(BYTE& Dest) {
		return Read(&Dest, 1);
	}
	BOOL ReadWord(WORD& Dest) {
		return Read(&Dest, 2);
	}
	BOOL ReadDword(DWORD& Dest) {
		return Read(&Dest, 4);
	}
};

class GzwFile : private BufferedFile, z_stream { // fastest possible gzip compression without pre-calculated size
public:
	GzwFile() {
		zalloc = Z_NULL;
		zfree = Z_NULL;
	}
	~GzwFile() {
		deflateEnd(this);
	}

	BOOL Create(CONST LPCTSTR FileName) {
		next_out = Buffer;
		avail_out = sizeof(Buffer);
		return deflateInit2(this, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 9, Z_DEFAULT_STRATEGY) == Z_OK && BufferedFile::Create(FileName);
	}
	BOOL Save() {
		while (deflate(this, Z_FINISH) != Z_STREAM_END) {
			if (avail_out || !File::Write(next_out = Buffer, avail_out = sizeof(Buffer))) {
				Delete(DeleteName);
				return FALSE;
			}
		}
		if (!File::Write(Buffer, sizeof(Buffer) - avail_out)) {
			Delete(DeleteName);
			return FALSE;
		}
		return TRUE;
	}
	BOOL Write(CONST LPCVOID Src, CONST DWORD Size) {
		next_in = LPBYTE(Src);
		avail_in = Size;
		do {
			if (deflate(this, Z_NO_FLUSH) != Z_OK || (!avail_out && !File::Write(next_out = Buffer, avail_out = sizeof(Buffer)))) {
				Delete(DeleteName);
				return FALSE;
			}
		} while (avail_in);
		return TRUE;
	}
	BOOL WriteByte(CONST BYTE Src) {
		return Write(&Src, 1);
	}
	BOOL WriteWord(CONST WORD Src) {
		return Write(&Src, 2);
	}
	BOOL WriteDword(CONST DWORD Src) {
		return Write(&Src, 4);
	}
};
