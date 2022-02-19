#pragma once
#include "framework.h"
#include "zlib.h"

class GenericFile {
protected:
	HANDLE File;

public:
	GenericFile(): File(INVALID_HANDLE_VALUE) {}
	~GenericFile() {
		CloseHandle(File);
	}

	DWORD GetSize() CONST {
		LARGE_INTEGER Size;
		if (!GetFileSizeEx(File, &Size)) {
			return 0;
		}
		if (Size.HighPart) {
			return INVALID_SET_FILE_POINTER;
		}
		return Size.LowPart;
	}
};

struct ReadingFile : public GenericFile {
	BOOL Open(CONST LPCTSTR FileName, CONST DWORD Flag) {
		File = CreateFile(FileName, FILE_READ_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, Flag, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		return File != INVALID_HANDLE_VALUE;
	}

	BOOL Skip(CONST DWORD Size) CONST {
		LONG High = 0;
		return SetFilePointer(File, Size, &High, FILE_CURRENT) != INVALID_SET_FILE_POINTER || GetLastError() == NO_ERROR;
	}
	BOOL Read(CONST LPVOID Data, CONST DWORD Size) CONST {
		DWORD Read;
		return ReadFile(File, Data, Size, &Read, NULL) && Read == Size;
	}

	BOOL ReadByte(BYTE &Data) CONST {
		return Read(&Data, 1);
	}
	BOOL ReadWord(WORD &Data) CONST {
		return Read(&Data, 2);
	}
	BOOL ReadDword(DWORD &Data) CONST {
		return Read(&Data, 4);
	}
};

struct WritingFile : public GenericFile {
	BOOL Open(CONST LPCTSTR FileName, CONST DWORD Flag) {
		File = CreateFile(FileName, FILE_WRITE_DATA | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, Flag, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		return File != INVALID_HANDLE_VALUE;
	}

	VOID Delete(CONST LPCTSTR FileName) {
		if (!CloseHandle(CreateFile(FileName, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL))) {
			SetFilePointer(File, 0, NULL, FILE_BEGIN);
			SetEndOfFile(File);
			CloseHandle(File);
			DeleteFile(FileName);
			File = INVALID_HANDLE_VALUE;
		}
	}

	VOID Append() CONST {
		LONG High = 0;
		SetFilePointer(File, 0, &High, FILE_END);
	}
	BOOL Write(CONST LPCVOID Data, CONST DWORD Size) CONST {
		DWORD Written;
		return WriteFile(File, Data, Size, &Written, NULL) && Written == Size;
	}

	BOOL WriteByte(CONST BYTE Data) CONST {
		return Write(&Data, 1);
	}
	BOOL WriteWord(CONST WORD Data) CONST {
		return Write(&Data, 2);
	}
	BOOL WriteDword(CONST DWORD Data) CONST {
		return Write(&Data, 4);
	}
};

class BufferedFile {
protected:
	LPBYTE Ptr;
	LPBYTE Data;
	LPBYTE End;

public:
	BufferedFile(): Ptr(NULL) {}
	~BufferedFile() {
		delete[] Ptr;
	}

	BOOL Start(CONST DWORD Size) {
		return BOOL(Data = Ptr = new(std::nothrow) BYTE[Size]);
	}
	VOID Write(CONST LPCVOID Src, CONST DWORD Size) {
		CopyMemory(Data, Src, Size);
		Data += Size;
	}
	VOID WriteByte(CONST BYTE Src) {
		return Write(&Src, 1);
	}
	VOID WriteWord(CONST WORD Src) {
		return Write(&Src, 2);
	}
	VOID WriteDword(CONST DWORD Src) {
		return Write(&Src, 4);
	}
	BOOL Save(CONST LPCTSTR FileName, CONST DWORD Flag) {
		WritingFile File;
		if (File.Open(FileName, Flag)) {
			if (File.Write(Ptr, Data - Ptr)) {
				return BOOL(End = Data);
			}
			File.Delete(FileName);
		}
		return FALSE;
	}

	BOOL Open(CONST LPCTSTR FileName, CONST DWORD Flag, CONST DWORD Size) {
		if (Start(Size)) {
			ReadingFile File;
			if (File.Open(FileName, Flag)) {
				if (File.Read(Data, Size)) {
					return BOOL(End = Data + Size);
				}
			}
		}
		return FALSE;
	}
	BOOL Open(CONST LPCTSTR FileName, CONST DWORD Flag) {
		ReadingFile File;
		if (File.Open(FileName, Flag)) {
			if (DWORD Size = File.GetSize()) {
				if (Start(Size)) {
					if (File.Read(Data, Size)) {
						return BOOL(End = Data + Size);
					}
				}
			}
		}
		return FALSE;
	}
	BOOL Reset(CONST DWORD Offset) {
		return BOOL(Data = Ptr + Offset);
	}
	LPBYTE Skip(CONST DWORD Size) {
		LPBYTE Result = Data;
		if ((Data += Size) > End) {
			return NULL;
		}
		return Result;
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
};

#define SZ_ERROR_DATA 1
extern "C" { // Modded LzmaLib for compression progress
	// *PropsSize must be = 5 // 0 <= Level <= 9, default = 5 // DictSize = 0, default to (1 << 24) // 0 <= lc <= 8, default = 3 // 0 <= lp <= 4, default = 0 // 0 <= pb <= 4, default = 2 // 5 <= fb <= 273, default = 32 // NumThreads = 1 or 2, default = 2
	INT __stdcall LzmaCompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD SrcLen, BYTE* Props, DWORD PropsSize, INT Level, DWORD DictSize, INT lc, INT lp, INT pb, INT fb, INT Threads, LPVOID Callback);
	INT __stdcall LzmaUncompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD* SrcLen, CONST BYTE* Props, DWORD PropsSize);
}

struct LzmaFile : public BufferedFile {
	BOOL StartHeader(CONST DWORD Size, DWORD Header) {
		return Start(Size + (Header += Size + 17)) ? BOOL(End = Data + Header) : FALSE;
	}
	VOID EndHeader() {
		Data = End;
	}
	BOOL Compress(CONST LPVOID Callback) {
		DWORD Size = Data - End;
		Data = End - Size - 8;
		WriteDword(Size);
		WriteDword(0);
		if (!LzmaCompress(Data, &Size, End, Size, Data - 13, 5, 9, 0, 3, 0, 2, 32, 4, Callback)) {
			*(DWORD*)(Data - 17) = Size + 13;
			return BOOL(Data += Size);
		}
		return FALSE;
	}
	BOOL Uncompress(CONST BOOL AllowTruncated) {
		DWORD OldSize;
		if (ReadDword(OldSize) && (Data + OldSize <= End || (AllowTruncated && (OldSize = End - Data)))) {
			if (CONST LPBYTE Props = Skip(5)) {
				DWORD Size, Large;
				if (ReadDword(Size) && Size && ReadDword(Large) && !Large) {
					End = Data;
					if (Data = new(std::nothrow) BYTE[Size]) {
						if (LzmaUncompress(Data, &Size, End, &(OldSize -= 13), Props, 5) != SZ_ERROR_DATA) {
							delete[] Ptr;
							Ptr = Data;
							return BOOL(End = Data + Size);
						}
						delete[] Data;
					}
				}
			}
		}
		return FALSE;
	}
};

class InflateFile : private BufferedFile, z_stream {
public:
	InflateFile() {
		zalloc = Z_NULL;
		zfree = Z_NULL;
	}
	~InflateFile() {
		inflateEnd(this);
	}

	BOOL Open(CONST LPCTSTR FileName, CONST DWORD Flag, CONST DWORD Size) {
		if (inflateInit2(this, MAX_WBITS + 16) == Z_OK) {
			if (BufferedFile::Open(FileName, Flag, Size)) {
				next_in = Data;
				avail_in = Size;
				return TRUE;
			}
		}
		return FALSE;
	}
	BOOL Open(CONST LPCTSTR FileName, CONST DWORD Flag) {
		if (inflateInit2(this, MAX_WBITS + 16) == Z_OK) {
			if (BufferedFile::Open(FileName, Flag)) {
				next_in = Data;
				avail_in = End - Data;
				return TRUE;
			}
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

class DeflateFile : public WritingFile, private z_stream {
	BYTE Buffer[0x20000];
public:
	DeflateFile() {
		zalloc = Z_NULL;
		zfree = Z_NULL;
	}
	~DeflateFile() {
		deflateEnd(this);
	}

	BOOL Open(CONST LPCTSTR FileName, CONST DWORD Flag) {
		if (deflateInit2(this, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 9, Z_DEFAULT_STRATEGY) == Z_OK) {
			if (WritingFile::Open(FileName, Flag)) {
				next_out = Buffer;
				avail_out = sizeof(Buffer);
				return TRUE;
			}
		}
		return FALSE;
	}
	BOOL Write() {
		while (deflate(this, Z_FINISH) != Z_STREAM_END) {
			if (avail_out || !WritingFile::Write(Buffer, sizeof(Buffer))) {
				return FALSE;
			}
			next_out = Buffer;
			avail_out = sizeof(Buffer);
		}
		return WritingFile::Write(Buffer, sizeof(Buffer) - avail_out);
	}
	BOOL Write(CONST LPCVOID Data, CONST DWORD Size) {
		next_in = LPBYTE(Data);
		avail_in = Size;
		do {
			if (deflate(this, Z_NO_FLUSH) != Z_OK) {
				return FALSE;
			}
			if (!avail_out) {
				if (!WritingFile::Write(Buffer, sizeof(Buffer))) {
					return FALSE;
				}
				next_out = Buffer;
				avail_out = sizeof(Buffer);
			}
		} while (avail_in);
		return TRUE;
	}
	BOOL WriteByte(CONST BYTE Data) {
		return Write(&Data, 1);
	}
	BOOL WriteWord(CONST WORD Data) {
		return Write(&Data, 2);
	}
	BOOL WriteDword(CONST DWORD Data) {
		return Write(&Data, 4);
	}
};
