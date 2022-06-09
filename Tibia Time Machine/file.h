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

	inline VOID Rewind(CONST DWORD Skip) CONST {
		if (SetFilePointer(Handle, Skip, NULL, FILE_BEGIN) != Skip) Delete();
	}
	inline VOID Save() CONST {
		if (!FlushFileBuffers(Handle)) Delete();
	}
	inline VOID Write(CONST LPCVOID Src, CONST DWORD Size) CONST {
		DWORD Written;
		if (!WriteFile(Handle, Src, Size, &Written, NULL) || Written != Size) Delete();
	}
	template <typename TYPE> inline VOID Write(CONST TYPE& Src) CONST {
		return Write(&Src, sizeof(TYPE));
	}
};

struct bad_read : public exception {};

class ReadingFile {
protected:
	HANDLE Handle;

public:
	inline ReadingFile(CONST LPCTSTR FileName) : Handle(CreateFile(FileName, FILE_READ_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL)) {
		if (Handle == INVALID_HANDLE_VALUE) throw bad_alloc();
	}
	inline ~ReadingFile() {
		CloseHandle(Handle);
	}

	inline DWORD GetSize() CONST {
		LARGE_INTEGER Size;
		return GetFileSizeEx(Handle, &Size) ? Size.HighPart ? INVALID_FILE_SIZE : Size.LowPart : 0;
	}
	inline BOOL Peek() CONST {
		return SetFilePointer(Handle, 0, NULL, FILE_CURRENT) < GetSize();
	}
	inline VOID Skip(CONST DWORD Size) CONST {
		if (SetFilePointer(Handle, Size, NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER) throw bad_read();
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
		return WritingFile::Save();
	}
	VOID Write(CONST LPCVOID Src, CONST DWORD Size) {
		if (Size > (sizeof(Buffer) - Pos)) {
			Flush();
		}
		if (Size >= sizeof(Buffer)) {
			return WritingFile::Write(Src, Size);
		}
		CopyMemory(Buffer + Pos, Src, Size);
		Pos += Size;
	}
	template <typename TYPE> inline VOID Write(CONST TYPE& Src) {
		return Write(&Src, sizeof(TYPE));
	}
};

class MappedFile {
	LPVOID Ptr;

protected:
	LPCBYTE Data;
	LPCBYTE End;

	inline VOID Realloc(CONST DWORD Size) {
		Ptr = VirtualAlloc(NULL, Size, MEM_TOP_DOWN | MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!Ptr) throw bad_alloc();
		End = (Data = LPCBYTE(Ptr)) + Size;
	}

public:
	inline MappedFile(CONST LPCTSTR FileName) { // memory mapping causes SEH, emulate instead
		ReadingFile File(FileName);
		if (DWORD Size = File.GetSize()) {
			Realloc(Size);
			try {
				File.Read(Ptr, End - Data);
				return;
			}
			catch (...) {
				this->~MappedFile();
			}
		}
		throw bad_alloc();
	}
	inline ~MappedFile() {
		VirtualFree(Ptr, NULL, MEM_RELEASE);
	}

	inline BOOL Peek() CONST {
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

extern "C" {
	typedef INT(*LzmaProgress)(LPVOID This, QWORD DecSize, QWORD EncSize, QWORD TotalSize); // Modded LzmaLib for compression progress and output to stream
	INT __stdcall LzmaCompress(LPBYTE Dest, LPDWORD DestLen, LPCBYTE Src, DWORD SrcLen, LPBYTE Props, DWORD PropsSize, INT Level, DWORD DictSize, INT lc, INT lp, INT pb, INT fb, INT Threads, LzmaProgress Callback);
	INT __stdcall LzmaUncompress(LPBYTE Dest, LPDWORD DestLen, LPCBYTE Src, LPDWORD SrcLen, LPCBYTE Props, DWORD PropsSize);
}

struct LzmaStream {
	DWORD(*WriteCallback)(LzmaStream* This, LPCVOID Src, DWORD Size);
};

class LzmaBufferedFile : private LzmaStream, WritingFile {
	DWORD Size;
	DWORD Skip;
	LPBYTE Buf;
	LPBYTE Data;

	static DWORD WriteThis(LzmaStream* This, LPCVOID Src, DWORD Size) {
		((LzmaBufferedFile*)This)->Size += Size;
		return WriteFile(((LzmaBufferedFile*)This)->Handle, Src, Size, &Size, NULL) ? Size : 0;
	}

public:
	inline LzmaBufferedFile(CONST LPCTSTR FileName, CONST DWORD Size, CONST DWORD Header): WritingFile(FileName), Skip(Header), Buf(LPBYTE(VirtualAlloc(NULL, Size + Header, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE))) {
		if (!Buf) Delete();
		Data = Buf + Size;
	}
	inline ~LzmaBufferedFile() {
		VirtualFree(Buf, NULL, MEM_RELEASE);
	}

	inline VOID Compress() {
		Data = Buf;
	}
	inline VOID Save(CONST LzmaProgress Callback) {
		WritingFile::Write(Data, Skip);
		WritingFile::Write(Size = 0);
		WriteCallback = WriteThis; // modded lzma api writes directly to the file
		for (DWORD Level = 9; LzmaCompress(LPBYTE(this), NULL, Buf, Data - Buf, NULL, 5, Level, 0, -1, -1, -1, -1, -1, Callback); Level--) {
			if (!Level) Delete();
			Rewind(Skip);
			WritingFile::Write(Size = 0);
		}
		Rewind(Skip);
		WritingFile::Write(Size);
		return WritingFile::Save();
	}
	inline VOID Write(CONST LPCVOID Src, CONST DWORD Size) {
		CopyMemory(Data, Src, Size);
		Data += Size;
	}
	template <typename TYPE> inline VOID Write(CONST TYPE Src) {
		*(*(TYPE**)&Data)++ = Src; // return Write(&Src, sizeof(TYPE));
	}
};

class LzmaMappedFile : public MappedFile {
public:
	inline LzmaMappedFile(CONST LPCTSTR FileName): MappedFile(FileName) { }

	inline VOID Uncompress(CONST BOOL AllowTruncated) {
		if (Data + Read<DWORD>() > End && !AllowTruncated) throw bad_read(); // very permissive about wrong sizes
		CONST LPCBYTE Props = Skip(5);
		DWORD Size, Compressed;
		if (!Read(Size) || Read<DWORD>() || !(Compressed = End - Data)) throw bad_read();
		MappedFile Temp(*this);
		Realloc(Size);
		if (LzmaUncompress(LPBYTE(Data), &Size, Temp.Skip(0), &Compressed, Props, 5)) {
			if (!Compressed)  throw bad_alloc();
			if (!Size || !AllowTruncated) throw bad_read();
		}
		End = Data + Size;
	}
};

class GzipBufferedFile : private z_stream, BufferedFile {
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
		return BufferedFile::Save();
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

class GzipMappedFile : private z_stream, MappedFile {
public:
	inline GzipMappedFile(CONST LPCTSTR FileName) : MappedFile(FileName) {
		zalloc = Z_NULL;
		zfree = Z_NULL;
		if (inflateInit2(this, MAX_WBITS + 16) != Z_OK) throw bad_alloc();
		avail_in = End - (next_in = LPBYTE(Data));
		avail_out = 0;
	}
	inline ~GzipMappedFile() {
		inflateEnd(this);
	}

	inline BOOL Peek() {
		return inflate(this, Z_NO_FLUSH) == Z_BUF_ERROR;
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
