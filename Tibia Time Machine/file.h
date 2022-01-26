#pragma once
#include "framework.h"
#include "zlib.h"

class GenericFile {
protected:
	HANDLE File;

public:
	GenericFile(): File(INVALID_HANDLE_VALUE) {}
	~GenericFile() {
		if (File != INVALID_HANDLE_VALUE) {
			CloseHandle(File);
		}
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
	BOOL Open(CONST LPCTSTR FileName, DWORD Flag) {
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
	BOOL Open(CONST LPCTSTR FileName, DWORD Flag) {
		File = CreateFile(FileName, FILE_WRITE_DATA | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, Flag, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		return File != INVALID_HANDLE_VALUE;
	}

	VOID Delete(CONST LPCTSTR FileName) {
		if (!CloseHandle(CreateFile(FileName, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL))) {
			SetFilePointer(File, 0, NULL, FILE_BEGIN);
			SetEndOfFile(File);
			CloseHandle(File);
			File = INVALID_HANDLE_VALUE;
			DeleteFile(FileName);
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

	LPBYTE Start(CONST DWORD Size) {
		return Data = Ptr = new(std::nothrow) BYTE[Size];
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
	LPBYTE Save(CONST LPCTSTR FileName) {
		WritingFile File;
		if (File.Open(FileName, CREATE_ALWAYS)) {
			if (File.Write(Ptr, Data - Ptr)) {
				return End = Data;
			}
			File.Delete(FileName);
		}
		return NULL;
	}

	LPBYTE Open(CONST LPCTSTR FileName) {
		ReadingFile File;
		if (File.Open(FileName, OPEN_EXISTING)) {
			if (DWORD Size = File.GetSize()) {
				if (Start(Size)) {
					if (File.Read(Data, Size)) {
						return End = Data + Size;
					}
				}
			}
		}
		return NULL;
	}
	LPBYTE Reset(CONST DWORD Offset) {
		return Data = Ptr + Offset;
	}
	LPBYTE Skip(CONST DWORD Size) {
		LPBYTE Result = Data;
		if ((Data += Size) > End) {
			return NULL;
		}
		return Result;
	}
	LPVOID Read(CONST LPVOID Dest, CONST DWORD Size) {
		if (LPBYTE Src = Skip(Size)) {
			return CopyMemory(Dest, Src, Size);
		}
		return NULL;
	}
	LPVOID ReadByte(BYTE& Dest) {
		return Read(&Dest, 1);
	}
	LPVOID ReadWord(WORD& Dest) {
		return Read(&Dest, 2);
	}
	LPVOID ReadDword(DWORD& Dest) {
		return Read(&Dest, 4);
	}
};

#define SZ_ERROR_DATA 1
extern "C" { // Modded LzmaLib for compression progress
	// *PropsSize must be = 5 // 0 <= Level <= 9, default = 5 // DictSize = 0, default to (1 << 24) // 0 <= lc <= 8, default = 3 // 0 <= lp <= 4, default = 0 // 0 <= pb <= 4, default = 2 // 5 <= fb <= 273, default = 32 // NumThreads = 1 or 2, default = 2
	INT __stdcall LzmaCompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD SrcLen, BYTE* Props, DWORD* PropsSize, INT Level, DWORD DictSize, INT lc, INT lp, INT pb, INT fb, INT Threads, LPVOID Callback);
	INT __stdcall LzmaUncompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD* SrcLen, CONST BYTE* Props, DWORD PropsSize);
}

struct LzmaFile : public BufferedFile {
	LPBYTE StartHeader(CONST DWORD Size, CONST DWORD Header) {
		if (Start(Header + 17 + Size * 2)) {
			return End = Data + Header + 17 + Size;
		}
		return NULL;
	}
	VOID EndHeader() {
		Data = End;
	}
	LPBYTE Compress(CONST LPVOID Callback) {
		DWORD Size = Data - End, PropsSize = 5;
		Data = End - Size - 8;
		WriteDword(Size);
		WriteDword(0);
		BYTE Props[5];
		if (!LzmaCompress(Data, &Size, End, Size, Props, &PropsSize, 5, 0, 3, 4, 2, 32, 4, Callback)) {
			Data -= 17;
			WriteDword(Size + 13);
			Write(Props, 5);
			return Data += Size + 8;
		}
		return NULL;
	}
	LPBYTE Uncompress(CONST BOOL AllowTruncated) {
		DWORD OldSize;
		if (ReadDword(OldSize) && OldSize > 13) {
			if (CONST LPBYTE Props = Skip(5)) {
				DWORD Size;
				if (ReadDword(Size) && Size) {
					DWORD Large;
					if (ReadDword(Large) && !Large) {
						if ((Data + (OldSize -= 13)) > End) {
							if (!AllowTruncated) {
								return NULL;
							}
							OldSize = End - Data;
						}
						End = Data;
						if (Data = new(std::nothrow) BYTE[Size]) {
							if (LzmaUncompress(Data, &Size, End, &OldSize, Props, 5) != SZ_ERROR_DATA) {
								delete[] Ptr;
								Ptr = Data;
								return End = Data + Size;
							}
							delete[] Data;
						}
					}
				}
			}
		}
		return NULL;
	}
};

class GzipFile {
	gzFile File;

public:
	GzipFile() : File(NULL) {}
	~GzipFile() {
		gzclose(File);
	}

	BOOL Open(CONST LPCSTR FileName, CONST LPCSTR Mode) {
		return BOOL(File = gzopen(FileName, Mode));
	}
	BOOL Open(CONST LPCWSTR FileName, CONST LPCSTR Mode) {
		CHAR FileNameA[MAX_PATH];
		CopyMemoryW(FileNameA, FileName, MAX_PATH);
		return Open(FileNameA, Mode);
	}
	BOOL Flush() {
		return gzflush(File, Z_FINISH) == Z_OK;
	}

	VOID Delete(CONST LPCTSTR FileName) {
		gzclose(File);
		DeleteFile(FileName);
		File = NULL;
	}

	BOOL Write(CONST LPCVOID Data, CONST DWORD Size) CONST {
		return gzwrite(File, Data, Size) == Size;
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

	BOOL Read(CONST LPVOID Data, CONST DWORD Size) CONST {
		return gzread(File, Data, Size) == Size;
	}
	BOOL ReadByte(BYTE& Data) CONST {
		return Read(&Data, 1);
	}
	BOOL ReadWord(WORD& Data) CONST {
		return Read(&Data, 2);
	}
	BOOL ReadDword(DWORD& Data) CONST {
		return Read(&Data, 4);
	}
};
