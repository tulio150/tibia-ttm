#pragma once
#include "framework.h"
#include "zlib.h"

class File {
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
		return (Handle = CreateFile(FileName, FILE_WRITE_DATA | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) != INVALID_HANDLE_VALUE;
	}
	BOOL Append(CONST LPCTSTR FileName) {
		if ((Handle = CreateFile(FileName, FILE_WRITE_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) != INVALID_HANDLE_VALUE) {
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

class BufferedFile : protected File {
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
		if ((!Pos || File::Write(Buffer, Pos)) && File::Save()) {
			return Pos = 0, TRUE;
		}
		return Delete(DeleteName), FALSE;
	}
	BOOL Write(CONST LPCVOID Src, CONST DWORD Size) {
		if (Size <= (sizeof(Buffer) - Pos)) {
			CopyMemory(Buffer + Pos, Src, Size);
			return Pos += Size, TRUE;
		}
		if (!Pos || File::Write(Buffer, Pos)) {
			if (Size < sizeof(Buffer)) {
				return BOOL(CopyMemory(Buffer, Src, Pos = Size));
			}
			if (File::Write(Src, Size)) {
				return Pos = 0, TRUE;
			}
		}
		return Delete(DeleteName), FALSE;
	}
	template <typename TYPE> BOOL Write(CONST TYPE Src) {
		return Write(&Src, sizeof(TYPE));
	}
};

class MappedFile : private File {
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
					Ptr = MapViewOfFile(Map, FILE_MAP_READ, NULL, NULL, Size);
					CloseHandle(Map);
					if (Ptr) {
						return BOOL(End = (Data = LPCBYTE(Ptr)) + Size);
					}
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

#define SZ_ERROR_INPUT_EOF 6
extern "C" { // Modded LzmaLib for compression progress
	// *PropsSize must be = 5 // 0 <= Level <= 9, default = 5 // DictSize = 0, default to (1 << 24) // 0 <= lc <= 8, default = 3 // 0 <= lp <= 4, default = 0 // 0 <= pb <= 4, default = 2 // 5 <= fb <= 273, default = 32 // NumThreads = 1 or 2, default = 2
	INT __stdcall LzmaCompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD SrcLen, BYTE* Props, DWORD PropsSize, INT Level, DWORD DictSize, INT lc, INT lp, INT pb, INT fb, INT Threads, LPCVOID Callback);
	INT __stdcall LzmaUncompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD* SrcLen, CONST BYTE* Props, DWORD PropsSize);
}

class LzmaBufferedFile : private File {
	DWORD Skip;
	LPBYTE Buf;
	LPBYTE Data;

public:
	LzmaBufferedFile() : Buf(NULL) {}
	~LzmaBufferedFile() {
		delete[] Buf;
	}

	BOOL Create(CONST LPCTSTR FileName, CONST DWORD Size, CONST DWORD Header) {
		if (Buf = new(nothrow) BYTE[Size * 2 + (Skip = Header + 17)]) {
			return Data = Buf + Size, File::Create(FileName);
		}
		return FALSE;
	}
	VOID Compress() {
		Data = Buf;
	}
	BOOL Save(CONST LPCTSTR FileName, CONST LPCVOID Callback) {
		DWORD Size = Data - Buf;
		Data += Skip;
		*(QWORD*)(Data - 8) = Size;
		if (!LzmaCompress(Data, &Size, Buf, Size, Data - 13, 5, 5, 0, 3, 0, 2, 32, 4, Callback)) {
			*(DWORD*)(Data - 17) = Size + 13;
			if (File::Write(Data - Skip, Size + Skip) && File::Save()) {
				return TRUE;
			}
		}
		return Delete(FileName), FALSE;
	}
	VOID Write(CONST LPCVOID Src, CONST DWORD Size) {
		CopyMemory(Data, Src, Size);
		Data += Size;
	}
	template <typename TYPE> VOID Write(CONST TYPE Src) {
		*(*(TYPE**)&Data)++ = Src; // return Write(&Src, sizeof(TYPE));
	}
};

class LzmaMappedFile : public MappedFile {
	LPBYTE Buf;

public:
	LzmaMappedFile() : Buf(NULL) {}
	~LzmaMappedFile() {
		delete[] Buf;
	}

	BOOL Uncompress(CONST BOOL AllowTruncated) {
		DWORD OldSize;
		if (Read(OldSize) && (Data + OldSize <= End || AllowTruncated)) { // very permissive about wrong sizes
			if (CONST LPCBYTE Props = Skip(5)) {
				DWORD Size, Large;
				if (Read(Size) && Size && Read(Large) && !Large && (Buf = new(nothrow) BYTE[Size])) {
					INT Error = LzmaUncompress(Buf, &Size, Data, &(OldSize = End - Data), Props, 5);
					if (!Error || (Error == SZ_ERROR_INPUT_EOF && AllowTruncated)) {
						return Unmap(), BOOL(End = (Data = Buf) + Size);
					}
				}
			}
		}
		return FALSE;
	}
};

class GzipBufferedFile : private BufferedFile, z_stream {
public:
	GzipBufferedFile() {
		zalloc = Z_NULL;
		zfree = Z_NULL;
	}
	~GzipBufferedFile() {
		deflateEnd(this);
	}

	BOOL Create(CONST LPCTSTR FileName) {
		next_out = Buffer;
		avail_out = sizeof(Buffer);
		return deflateInit2(this, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 9, Z_DEFAULT_STRATEGY) == Z_OK && BufferedFile::Create(FileName);
	}
	BOOL Save() {
		do {
			if (deflate(this, Z_FINISH) == Z_STREAM_END) {
				if (File::Write(Buffer, sizeof(Buffer) - avail_out) && File::Save()) {
					return TRUE;
				}
				break;
			}
		} while (!avail_out && File::Write(next_out = Buffer, avail_out = sizeof(Buffer)));
		return Delete(DeleteName), FALSE;
	}
	BOOL Write(CONST LPCVOID Src, CONST DWORD Size) {
		next_in = LPBYTE(Src);
		avail_in = Size;
		while (deflate(this, Z_NO_FLUSH) == Z_OK && (avail_out || File::Write(next_out = Buffer, avail_out = sizeof(Buffer)))) {
			if (!avail_in) {
				return TRUE;
			}
		}
		return Delete(DeleteName), FALSE;
	}
	template <typename TYPE> BOOL Write(CONST TYPE Src) {
		return Write(&Src, sizeof(TYPE));
	}
};

class GzipMappedFile : private MappedFile, z_stream {
public:
	GzipMappedFile() {
		zalloc = Z_NULL;
		zfree = Z_NULL;
	}
	~GzipMappedFile() {
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
