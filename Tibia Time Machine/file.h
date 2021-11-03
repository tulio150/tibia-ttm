#pragma once
#include "framework.h"

extern "C" {
	INT __stdcall LzmaCompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD SrcLen, BYTE* Props, DWORD* PropsSize, INT Level, DWORD DictSize, INT lc, INT lp, INT pb, INT fb, INT numThreads);
	/* *PropsSize must be = 5
	0 <= Level <= 9, default = 5
	DictSize = 0, default to (1 << 24)
	0 <= lc <= 8, default = 3
	0 <= lp <= 4, default = 0
	0 <= pb <= 4, default = 2
	5 <= fb <= 273, default = 32
	NumThreads = 1 or 2, default = 2 */
	INT __stdcall LzmaUncompress(BYTE* Dest, DWORD* DestLen, CONST BYTE* Src, DWORD* SrcLen, CONST BYTE* Props, DWORD PropsSize);
}

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

struct ReadingFile: public GenericFile {
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

struct WritingFile: public GenericFile {
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
	VOID Reset(CONST DWORD Pos) {
		Data = Ptr + Pos;
	}
	LPBYTE Skip(CONST DWORD Size) {
		if (Data + Size > End) {
			return NULL;
		}
		LPBYTE Result = Data;
		Data += Size;
		return Result;
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
	BOOL Read(CONST LPVOID Dest, CONST DWORD Size) {
		if (Data + Size > End) {
			return FALSE;
		}
		CopyMemory(Dest, Data, Size);
		Data += Size;
		return TRUE;
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

	BOOL Save(CONST LPCTSTR FileName) {
		WritingFile File;
		if (File.Open(FileName, CREATE_ALWAYS)) {
			if (File.Write(Ptr, Data - Ptr)) {
				return TRUE;
			}
			File.Delete(FileName);
		}
		return FALSE;
	}
	DWORD Open(CONST LPCTSTR FileName) {
		ReadingFile File;
		if (File.Open(FileName, OPEN_EXISTING)) {
			DWORD Size = File.GetSize();
			if (Size) {
				if (Start(Size + 1)) {
					if (File.Read(Data, Size)) {
						End = Data + Size;
						return Size;
					}
				}
			}
		}
		return 0;
	}

	LPBYTE LZMA_Start(CONST DWORD Size) {
		return Start(Size * 2);
	}
	BOOL LZMA_Compress(CONST WritingFile& File) {
		BYTE Props[5];
		DWORD PropsSize = 5;
		DWORD UncompressedSize = Data - Ptr;
		DWORD CompressedSize = UncompressedSize;
		if (!LzmaCompress(Data, &CompressedSize, Ptr, UncompressedSize, Props, &PropsSize, 5, 0, 3, 4, 2, 32, 4)) {
			if (File.WriteDword(CompressedSize + 13)) {
				if (File.Write(Props, 5)) {
					if (File.WriteDword(UncompressedSize)) {
						if (File.WriteDword(0)) {
							if (File.Write(Data, CompressedSize)) {
								return TRUE;
							}
						}
					}
				}
			}
		}
		return FALSE;
	}
	DWORD LZMA_Decompress() {
		DWORD CompressedSize;
		if (ReadDword(CompressedSize) && CompressedSize > 13) {
			CONST LPBYTE Props = Skip(5);
			if (Props) {
				DWORD UncompressedSize;
				if (ReadDword(UncompressedSize) && UncompressedSize) {
					DWORD LargeFile;
					if (ReadDword(LargeFile) && !LargeFile) {
						LPBYTE Compressed = Skip(CompressedSize -= 13);
						if (Compressed) {
							Data = new(std::nothrow) BYTE[UncompressedSize];
							if (Data) {
								DWORD TotalUncompressed = UncompressedSize, TotalCompressed = CompressedSize;
								if (!LzmaUncompress(Data, &TotalUncompressed, Compressed, &TotalCompressed, Props, 5)) {
									if (TotalUncompressed == UncompressedSize && TotalCompressed == CompressedSize) {
										delete[] Ptr;
										Ptr = Data;
										End = Data + UncompressedSize;
										return UncompressedSize;
									}
								}
								delete[] Data;
							}
						}
					}
				}
			}
		}
		return 0;
	}
};