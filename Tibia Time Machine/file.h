#pragma once
#include "framework.h"
#include "zlib.h"

class File { // generic file read/write
protected:
	HANDLE Handle;

	DWORD GetSize() CONST {
		LARGE_INTEGER Size;
		return GetFileSizeEx(Handle, &Size) ? Size.HighPart ? INVALID_FILE_SIZE : Size.LowPart : 0;
	}
	VOID Delete(CONST LPCTSTR FileName) {
		HANDLE(WINAPI * ReOpenFile)(HANDLE, DWORD, DWORD, DWORD) = (HANDLE(WINAPI*)(HANDLE, DWORD, DWORD, DWORD)) GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "ReOpenFile");
		if (!CloseHandle(ReOpenFile ? ReOpenFile(Handle, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_FLAG_DELETE_ON_CLOSE) : CreateFile(FileName, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL))) {
			DeleteFile(FileName);
			SetFilePointer(Handle, 0, NULL, FILE_BEGIN);
			SetEndOfFile(Handle);
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
	BOOL Skip(CONST DWORD Size) CONST {
		LARGE_INTEGER Position = { Size, 0 };
		return SetFilePointerEx(Handle, Position, NULL, FILE_CURRENT);
	}
	BOOL Read(CONST LPVOID Dest, CONST DWORD Size) CONST {
		DWORD Read;
		return ReadFile(Handle, Dest, Size, &Read, NULL) && Read == Size;
	}
	template <typename TYPE> BOOL Read(TYPE& Dest) CONST {
		return Read(&Dest, sizeof(TYPE));
	}

	BOOL Create(CONST LPCTSTR FileName) {
		return (Handle = CreateFile(FileName, FILE_READ_DATA | FILE_WRITE_DATA | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) != INVALID_HANDLE_VALUE;
	}
	BOOL Append(CONST LPCTSTR FileName) {
		if ((Handle = CreateFile(FileName, FILE_WRITE_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) != INVALID_HANDLE_VALUE) {
			SetFilePointer(Handle, 0, NULL, FILE_END);
			return TRUE;
		}
		return FALSE;
	}
	BOOL Save() {
		return FlushFileBuffers(Handle);
	}
	BOOL Write(CONST LPCVOID Src, CONST DWORD Size) CONST {
		DWORD Written;
		return WriteFile(Handle, Src, Size, &Written, NULL) && Written == Size;
	}
	template <typename TYPE> BOOL Write(CONST TYPE Src) CONST {
		return Write(&Src, sizeof(TYPE));
	}
};

class BufferedFile : protected File { // uses a stack buffer to speed up writes, does not need file size in advance
protected:
	LPCTSTR DeleteName;
	DWORD Pos;
	BYTE Buffer[0x20000];

public:
	BufferedFile() : Pos(0), DeleteName(NULL) {}

	BOOL Create(CONST LPCTSTR FileName) {
		return File::Create(DeleteName = FileName);
	}
	BOOL Save() {
		if (Pos && !File::Write(Buffer, Pos) || !File::Save()) {
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
	template <typename TYPE> BOOL Write(CONST TYPE Src) {
		return Write(&Src, sizeof(TYPE));
	}
};

class MappedFile : protected File { // uses memory mapping to speed up reads, fails on very big files
	LPCVOID Ptr;

protected:
	LPCBYTE Data;
	LPCBYTE End;

	VOID Unmap() {
		UnmapViewOfFile(Ptr);
		Ptr = NULL;
	}

public:
	MappedFile(): Ptr(NULL) {}
	~MappedFile() {
		UnmapViewOfFile(Ptr);
	}

	BOOL Open(CONST LPCTSTR FileName) {
		if (File::Open(FileName)) {
			if (DWORD Size = GetSize()) {
				if (HANDLE Map = CreateFileMapping(Handle, NULL, PAGE_READONLY, NULL, Size, NULL)) {
					if (Ptr = MapViewOfFile(Map, FILE_MAP_READ, NULL, NULL, Size)) {
						CloseHandle(Map);
						return BOOL(End = (Data = LPCBYTE(Ptr)) + Size);
					}
					CloseHandle(Map);
				}
			}
		}
		return FALSE;
	}
	LPCBYTE Skip(CONST DWORD Size) {
		LPCBYTE Result = Data;
		return (Data += Size) <= End ? Result : NULL;
	}
	BOOL Read(CONST LPVOID Dest, CONST DWORD Size) {
		if (LPCBYTE Src = Skip(Size)) {
			return BOOL(CopyMemory(Dest, Src, Size));
		}
		return FALSE;
	}
	template <typename TYPE> BOOL Read(TYPE& Dest) {
		return Read(&Dest, sizeof(TYPE));
	}
};

#define SZ_ERROR_DATA 1
extern "C" { // Modded LzmaLib for compression progress
	// *PropsSize must be = 5 // 0 <= Level <= 9, default = 5 // DictSize = 0, default to (1 << 24) // 0 <= lc <= 8, default = 3 // 0 <= lp <= 4, default = 0 // 0 <= pb <= 4, default = 2 // 5 <= fb <= 273, default = 32 // NumThreads = 1 or 2, default = 2
	INT __stdcall LzmaCompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD SrcLen, BYTE* Props, DWORD PropsSize, INT Level, DWORD DictSize, INT lc, INT lp, INT pb, INT fb, INT Threads, LPCVOID Callback);
	INT __stdcall LzmaUncompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD* SrcLen, CONST BYTE* Props, DWORD PropsSize);
}

class LzmarFile : public MappedFile { // fastest possible single-pass lzma decompression
	LPBYTE Buf;

public:
	LzmarFile() : Buf(NULL) {}
	~LzmarFile() {
		delete[] Buf;
	}

	BOOL Uncompress(CONST BOOL AllowTruncated) {
		DWORD OldSize;
		if (Read(OldSize) && (Data + OldSize <= End || AllowTruncated && (OldSize = End - Data))) {
			if (CONST LPCBYTE Props = Skip(5)) {
				DWORD Size, Large;
				if (Read(Size) && Size && Read(Large) && !Large) {
					if (Buf = new(std::nothrow) BYTE[Size]) {
						if (LzmaUncompress(Buf, &Size, Data, &(OldSize -= 13), Props, 5) != SZ_ERROR_DATA) {
							Unmap(); // compressed file not needed anymore, free some memory
							return BOOL(End = (Data = Buf) + Size);
						}
					}
				}
			}
		}
		return FALSE;
	}
};

class LzmawFile : public File { // slow, uses a lot of memory, and needs the size pre-calculated
	LPBYTE Data;
	LPBYTE Temp;
	LPBYTE Buf;

public:
	LzmawFile() : Buf(NULL) {}
	~LzmawFile() {
		delete[] Buf;
	}

	BOOL Create(CONST LPCTSTR FileName, CONST DWORD Size, DWORD Header) {
		if (Buf = new(std::nothrow) BYTE[Size + (Header += Size + 17)]) {
			Temp = (Data = Buf) + Header;
			return File::Create(FileName);
		}
		return FALSE;
	}
	VOID Compress() {
		Data = Temp;
	}
	BOOL Save(CONST LPCTSTR FileName, CONST LPCVOID Callback) {
		DWORD Size = Data - Temp;
		Data = Temp - Size;
		*(QWORD*)(Data - 8) = Size;
		if (!LzmaCompress(Data, &Size, Temp, Size, Data - 13, 5, 5, 0, 3, 0, 2, 32, 4, Callback)) {
			*(DWORD*)(Data - 17) = Size + 13;
			if (File::Write(Buf, Data - Buf + Size) && File::Save()) { // not mapped because it's a single write
				return TRUE;
			}
		}
		Delete(FileName);
		return FALSE;
	}
	VOID Write(CONST LPCVOID Src, CONST DWORD Size) {
		CopyMemory(Data, Src, Size);
		Data += Size;
	}
	template <typename TYPE> VOID Write(CONST TYPE Src) {
		*(*(TYPE**)&Data)++ = Src;
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
			next_in = LPBYTE(Data);
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
	template <typename TYPE> BOOL Read(TYPE& Dest) {
		return Read(&Dest, sizeof(TYPE));
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
		if (!File::Write(Buffer, sizeof(Buffer) - avail_out) || !File::Save()) {
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
	template <typename TYPE> BOOL Write(CONST TYPE Src) {
		return Write(&Src, sizeof(TYPE));
	}
};
