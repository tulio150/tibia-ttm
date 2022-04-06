#pragma once
#include "framework.h"
#include "zlib.h"

class WritingFile {
protected:
	HANDLE Handle;

	VOID Delete() CONST {
		if (HANDLE(WINAPI * ReOpenFile)(HANDLE, DWORD, DWORD, DWORD) = (HANDLE(WINAPI*)(HANDLE, DWORD, DWORD, DWORD)) GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "ReOpenFile")) {
			CloseHandle(ReOpenFile(Handle, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_FLAG_DELETE_ON_CLOSE));
		}
		throw bad_alloc();
	}

public:
	inline WritingFile(CONST LPCTSTR FileName) : Handle(CreateFile(FileName, FILE_WRITE_DATA | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) {
		if (Handle == INVALID_HANDLE_VALUE) throw bad_alloc();
	}
	inline WritingFile(CONST LPCTSTR FileName, CONST DWORD Start) : Handle(CreateFile(FileName, FILE_WRITE_DATA | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) {
		if (Handle == INVALID_HANDLE_VALUE) throw bad_alloc();
		SetFilePointer(Handle, 0, NULL, Start);
	}
	inline ~WritingFile() {
		CloseHandle(Handle);
	}

	inline VOID Save() CONST {
		if (!FlushFileBuffers(Handle)) Delete();
	}
	inline VOID Write(CONST LPCVOID Src, CONST DWORD Size) CONST {
		DWORD Written;
		if (!WriteFile(Handle, Src, Size, &Written, NULL) || Written != Size) Delete();
	}
	template <typename TYPE> inline VOID Write(CONST TYPE& Src) CONST {
		Write(&Src, sizeof(TYPE));
	}
};

struct bad_read : public exception {};

class ReadingFile {
protected:
	HANDLE Handle;

	inline DWORD GetSize() CONST {
		LARGE_INTEGER Size;
		return GetFileSizeEx(Handle, &Size) ? Size.HighPart ? INVALID_FILE_SIZE : Size.LowPart : 0;
	}

public:
	inline ReadingFile(CONST LPCTSTR FileName) : Handle(CreateFile(FileName, FILE_READ_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) {
		if (Handle == INVALID_HANDLE_VALUE) throw bad_alloc();
	}
	inline ~ReadingFile() {
		CloseHandle(Handle);
	}

	inline BOOL Peek() CONST {
		return SetFilePointer(Handle, 0, NULL, SEEK_CUR) < GetSize();
	}
	inline VOID Skip(CONST DWORD Size) CONST {
		LARGE_INTEGER Position = { Size, 0 };
		if (!SetFilePointerEx(Handle, Position, NULL, FILE_CURRENT)) throw bad_read();
	}
	inline VOID Read(CONST LPVOID Dest, CONST DWORD Size) CONST {
		DWORD Read;
		if (!ReadFile(Handle, Dest, Size, &Read, NULL)) throw bad_alloc();
		if (Read != Size) throw bad_read();
	}
	template <typename TYPE> inline TYPE& Read(TYPE& Dest) CONST {
		Read(&Dest, sizeof(TYPE));
		return Dest;
	}
	template <typename TYPE> inline TYPE Read() CONST {
		TYPE Result;
		return Read(Result);
	}
};

class BufferedFile : protected WritingFile {
protected:
	DWORD Pos;
	BYTE Buffer[0x20000];

	VOID Flush() {
		if (Pos) {
			WritingFile::Write(Buffer, Pos);
			Pos = 0;
		}
	}

public:
	inline BufferedFile(CONST LPCTSTR FileName) : WritingFile(FileName), Pos(0) {}

	inline VOID Save() {
		Flush();
		WritingFile::Save();
	}
	VOID Write(CONST LPCVOID Src, CONST DWORD Size) {
		if (Size > (sizeof(Buffer) - Pos)) {
			Flush();
			if (Size >= sizeof(Buffer)) {
				return WritingFile::Write(Src, Size);
			}
		}
		CopyMemory(Buffer + Pos, Src, Size);
		Pos += Size;
	}
	template <typename TYPE> inline VOID Write(CONST TYPE& Src) {
		Write(&Src, sizeof(TYPE));
	}
};

class MappedFile : private ReadingFile {
	LPVOID Filter;
	LPCVOID Ptr;

	static LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS* Info) {
		if (Info->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
			if (Info->ExceptionRecord->NumberParameters > 1) {
				if (Info->ExceptionRecord->ExceptionInformation[0] == EXCEPTION_READ_FAULT) {
					throw bad_alloc();
				}
			}
		}
		return EXCEPTION_CONTINUE_SEARCH;
	}

protected:
	LPCBYTE Data;
	LPCBYTE End;

	inline VOID Unmap() {
		RemoveVectoredExceptionHandler(Filter);
		Filter = NULL;
		UnmapViewOfFile(Ptr);
		Ptr = NULL;
	}

public:
	inline MappedFile(CONST LPCTSTR FileName) : ReadingFile(FileName) {
		CONST DWORD Size = GetSize();
		if (!Size) throw bad_alloc();
		CONST HANDLE Map = CreateFileMapping(Handle, NULL, PAGE_READONLY, NULL, Size, NULL);
		if (!Map) throw bad_alloc();
		Ptr = MapViewOfFile(Map, FILE_MAP_READ, NULL, NULL, Size);
		CloseHandle(Map);
		if (!Ptr) throw bad_alloc();
		End = (Data = LPCBYTE(Ptr)) + Size;
		Filter = AddVectoredExceptionHandler(FALSE, ExceptionFilter);
	}
	inline ~MappedFile() {
		RemoveVectoredExceptionHandler(Filter);
		UnmapViewOfFile(Ptr);
	}

	inline BOOL Peek() {
		return Data < End;
	}
	inline LPCBYTE Skip(CONST DWORD Size) {
		LPCBYTE Src = Data;
		if ((Data += Size) > End) throw bad_read();
		return Src;
	}
	inline VOID Read(CONST LPVOID Dest, CONST DWORD Size) {
		CopyMemory(Dest, Skip(Size), Size);
	}
	template <typename TYPE> inline CONST TYPE& Read() {
		if ((Data + sizeof(TYPE)) > End) throw bad_read();
		return *(*(CONST TYPE**)&Data)++;
	}
	template <typename TYPE> inline TYPE& Read(TYPE& Dest) {
		return Dest = Read<TYPE>();
	}
};

extern "C" { // Modded LzmaLib for compression progress
	// *PropsSize must be = 5 // 0 <= Level <= 9, default = 5 // DictSize = 0, default to (1 << 24) // 0 <= lc <= 8, default = 3 // 0 <= lp <= 4, default = 0 // 0 <= pb <= 4, default = 2 // 5 <= fb <= 273, default = 32 // NumThreads = 1 or 2, default = 2
	INT __stdcall LzmaCompress(LPBYTE Dest, LPDWORD DestLen, LPCBYTE Src, DWORD SrcLen, LPBYTE Props, DWORD PropsSize, INT Level, DWORD DictSize, INT lc, INT lp, INT pb, INT fb, INT Threads, LPCVOID Callback);
	INT __stdcall LzmaUncompress(LPBYTE Dest, LPDWORD DestLen, LPCBYTE Src, LPDWORD SrcLen, LPCBYTE Props, DWORD PropsSize);
}

class LzmaBuffer {
protected:
	LPBYTE Buf;

public:
	inline LzmaBuffer(CONST LPBYTE New): Buf(New) { }
	inline ~LzmaBuffer() {
		delete[] Buf;
	}
};

class LzmaBufferedFile : private LzmaBuffer, WritingFile {
	DWORD Skip;
	LPBYTE Data;

public:
	inline LzmaBufferedFile(CONST LPCTSTR FileName, CONST DWORD Size, CONST DWORD Header): LzmaBuffer(new BYTE[Size * 2 + Header + 17]), WritingFile(FileName), Skip(Header + 17), Data(Buf + Size) { }

	inline VOID Compress() {
		Data = Buf;
	}
	inline VOID Save(CONST LPCVOID Callback) {
		DWORD Size = Data - Buf;
		Data += Skip;
		*(QWORD*)(Data - 8) = Size;
		if (LzmaCompress(Data, &Size, Buf, Size, Data - 13, 5, 5, 0, 3, 0, 2, 32, 4, Callback)) Delete();
		*(DWORD*)(Data - 17) = Size + 13;
		WritingFile::Write(Data - Skip, Size + Skip);
		WritingFile::Save();
	}
	inline VOID Write(CONST LPCVOID Src, CONST DWORD Size) {
		CopyMemory(Data, Src, Size);
		Data += Size;
	}
	template <typename TYPE> inline VOID Write(CONST TYPE Src) {
		*(*(TYPE**)&Data)++ = Src; // Write(&Src, sizeof(TYPE));
	}
};

class LzmaMappedFile : private LzmaBuffer, public MappedFile {
public:
	inline LzmaMappedFile(CONST LPCTSTR FileName): LzmaBuffer(NULL), MappedFile(FileName) {}

	inline VOID Uncompress(CONST BOOL AllowTruncated) {
		DWORD OldSize = Read<DWORD>();
		if (Data + OldSize > End && !AllowTruncated) throw bad_read(); // very permissive about wrong sizes
		CONST LPCBYTE Props = Skip(5);
		DWORD Size = Read<DWORD>();
		DWORD Large = Read<DWORD>();
		if (!Size || Large || !(OldSize = End - Data)) throw bad_read();
		if (LzmaUncompress(Buf = new BYTE[Size], &Size, Data, &OldSize, Props, 5)) { // exception inside here leaves lzma data leak
			if (!OldSize) throw bad_alloc();
			if (!Size || !AllowTruncated) throw bad_read();
		}
		End = (Data = Buf) + Size;
		Unmap();
	}
};

class GzipBufferedFile : private BufferedFile, z_stream {
public:
	inline GzipBufferedFile(CONST LPCTSTR FileName) : BufferedFile(FileName) {
		zalloc = Z_NULL;
		zfree = Z_NULL;
		if (deflateInit2(this, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 9, Z_DEFAULT_STRATEGY) != Z_OK) Delete();
		next_out = Buffer;
		avail_out = sizeof(Buffer);
	}
	inline ~GzipBufferedFile() {
		deflateEnd(this);
	}

	inline VOID Save() {
		while (deflate(this, Z_FINISH) != Z_STREAM_END) {
			if (avail_out) Delete();
			WritingFile::Write(next_out = Buffer, avail_out = sizeof(Buffer));
		}
		Pos = sizeof(Buffer) - avail_out;
		BufferedFile::Save();
	}
	VOID Write(CONST LPCVOID Src, CONST DWORD Size) {
		next_in = LPBYTE(Src);
		avail_in = Size;
		do {
			if (deflate(this, Z_NO_FLUSH) != Z_OK) Delete();
			if (!avail_out) WritingFile::Write(next_out = Buffer, avail_out = sizeof(Buffer));
		} while (avail_in);
	}
	template <typename TYPE> inline VOID Write(CONST TYPE Src) {
		return Write(&Src, sizeof(TYPE));
	}
};

class GzipMappedFile : private MappedFile, z_stream {
public:
	inline GzipMappedFile(CONST LPCTSTR FileName) : MappedFile(FileName) {
		zalloc = Z_NULL;
		zfree = Z_NULL;
		if (inflateInit2(this, MAX_WBITS + 16) != Z_OK) throw bad_alloc();
		next_in = LPBYTE(Data);
		avail_in = End - Data;
		avail_out = 0;
	}
	inline ~GzipMappedFile() {
		inflateEnd(this);
	}

	inline BOOL Peek() {
		return inflate(this, Z_NO_FLUSH) != Z_STREAM_END;
	}
	VOID Read(CONST LPVOID Dest, CONST DWORD Size) {
		if (avail_out = Size) {
			next_out = LPBYTE(Dest);
			if (inflate(this, Z_SYNC_FLUSH) < Z_OK) throw bad_alloc();
			if (avail_out) throw bad_read();
		}
	}
	template <typename TYPE> inline TYPE& Read(TYPE& Dest) {
		Read(&Dest, sizeof(TYPE));
		return Dest;
	}
	template <typename TYPE> inline TYPE Read() {
		TYPE Result;
		return Read(Result);
	}
};
