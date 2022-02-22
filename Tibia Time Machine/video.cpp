#include "video.h"
#include "main.h"
#include "file.h"
#include "tibia.h"
#include "proxy.h"
#include "loader.h"

#define CAM_HASH "TTM created CAM file - no hash."
#define BIGDELAY 0xFFFF
#define IDLETIME 10000
#define PAUSED (-4)
#define LIGHTSPEED 10
#define CalcDelay(Delay) (Speed > 0 ? ((Delay) >> Speed) + 1 : ((Delay) << -Speed))
#define CalcElapsed(Elapsed) (Speed < 0 ? ((Elapsed) >> -Speed) + 1 : (Elapsed) << Speed)

namespace Video {
	STATE State = IDLE;

	Packet *First = NULL;
	Packet *Last = NULL;
	Packet *Current;

	TCHAR FileName[MAX_PATH] = _T("");
	BYTE Changed = FALSE;

	DWORD PlayedTime;
	INT Speed = 0;
	CONST TCHAR SpeedLabel[][6] = { _T(" x2"), _T(" x4"), _T(" x8"), _T(" x16"), _T(" x32"), _T(" x64"), _T(" x128"), _T(" x256"), _T(" x512") };

	HCRYPTKEY RecKey = NULL;

#define CurrentLogin ((Session *) Current)

	VOID Packet::EndSession()  {
		Login->Last = this;
	}
	VOID Packet::CutSession() {
		((Session *)Next)->Prev = this;
	}

	DWORD Packet::TimeInSession() CONST {
		return Time - Login->Time;
	}
	BOOL Packet::IsLast() CONST {
		return this == Login->Last;
	}
	BOOL Packet::NeedEncrypt() {
		if (this == Login->Encrypt) {
			Login->Encrypt = Next;
			return TRUE;
		}
		return FALSE;
	}
	BOOL Packet::NeedDecrypt() {
		if (this == Login->Encrypt) {
			Current = Login->Last;
			Login->Encrypt = Login;
			return FALSE;
		}
		if (this == Login->Last) {
			Login->Encrypt = Login;
		}
		return TRUE;
	}

	namespace Timer {
		DWORD Last;
		TIMECAPS TimeCaps;

		VOID Setup() {
			timeGetDevCaps(&TimeCaps, sizeof(TIMECAPS));
		}
		VOID Restart() {
			Last = timeGetTime();
		}
		VOID Start() {
			timeBeginPeriod(TimeCaps.wPeriodMin);
			Restart();
		}
		DWORD Partial() {
			return timeGetTime() - Last;
		}
		DWORD Elapsed() {
			DWORD Saved = Last;
			Restart();
			return Last - Saved;
		}
		VOID Stop() {
			timeEndPeriod(TimeCaps.wPeriodMin);
		}
	}

	VOID HandleTibiaClosed() {
		switch (State) {
			case WAIT:
				return Abort();
			case RECORD:
				return Stop();
			case PLAY:
			case SCROLL:
				return Eject();
		}
	}

	VOID WINAPIV ThreadUnload(Packet *Current) {
		Packet *Next;
		do {
			Next = Current->Next;
			delete Current;
			SwitchToThread();
		} while (Current = Next);
	}
	VOID Unload() {
		if (_beginthread(_beginthread_proc_type(ThreadUnload), 0, Current) == -1L) {
			ThreadUnload(Current); //out of memory, let's free some from our own thread
		}
	}

	VOID SetFileTitle() {
		TCHAR Title[MAX_PATH];
		LPTSTR Name = PathFindFileName(FileName);
		SIZE_T NameLen = PathFindExtension(Name) - Name;
		if (NameLen > MAX_PATH - countof(MainWnd::Title)) {
			NameLen = MAX_PATH - countof(MainWnd::Title);
		}
		CopyMemory(Title, Name, TLEN(NameLen));
		CopyMemory(Title + NameLen, _T(" - "), TLEN(3));
		CopyMemory(Title + NameLen + 3, MainWnd::Title, sizeof(MainWnd::Title));
		SetWindowText(MainWnd::Handle, Title);
	}
	VOID ClearFileTitle() {
		SetWindowText(MainWnd::Handle, MainWnd::Title);
	}

	UINT Save() {
		WritingFile File; // avoid BufferedFile for the ability to save under low memory, and to avoid size calculations
		if (!File.Open(FileName, CREATE_ALWAYS)) {
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.WriteWord(Tibia::Version)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.WriteByte(Tibia::HostLen)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (Tibia::HostLen) {
			if (!File.Write(Tibia::Host, Tibia::HostLen)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			if (!File.WriteWord(Tibia::Port)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
		}
		if (!File.WriteDword(Last->Time)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		NeedParser ToSave;
		PacketData *Packet = Parser->GetPacketData(*(Current = First));
		if (!File.Write(Packet, Packet->RawSize())) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		MainWnd::Progress_Set(0, Last->Time);
		while (Current->Next) {
			if (Current->IsLast()) {
				if (!File.WriteByte(TRUE)) {
					File.Delete(FileName);
					return ERROR_CANNOT_SAVE_VIDEO_FILE;
				}
			}
			else {
				if (!File.WriteByte(FALSE)) {
					File.Delete(FileName);
					return ERROR_CANNOT_SAVE_VIDEO_FILE;
				}
				if (!File.WriteWord(WORD(Current->Next->Time - Current->Time))) {
					File.Delete(FileName);
					return ERROR_CANNOT_SAVE_VIDEO_FILE;
				}
			}
			Packet = Parser->GetPacketData(*(Current = Current->Next));
			if (!File.Write(Packet, Packet->RawSize())) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			MainWnd::Progress_Set(Current->Time, Last->Time);
		}
		Changed = FALSE;
		return NULL;
	}

	UINT BeforeOpen(BOOL &Override, CONST HWND Parent, CONST WORD Version, CONST BYTE HostLen, CONST LPCSTR Host, CONST WORD Port) {
		if (Override) {
			if (!Last && !Tibia::Running) {
				Tibia::SetHost(Version, HostLen, Host, Port);
				MainWnd::Progress_Pause();
				if (!Loader().Run(Parent, TITLE_LOADER_OVERRIDE)) {
					if (Version < 700 || Version > LATEST) {
						Tibia::Version = Version < 700 ? 700 : LATEST;
						return ERROR_UNSUPPORTED_TIBIA_VERSION;
					}
					Tibia::SetVersionString(Version);
					Override = FALSE;
				}
				MainWnd::Progress_Start();
			}
		}
		else {
			if (!Last && !Tibia::Running) {
				if (Version < 700 || Version > LATEST) {
					return ERROR_UNSUPPORTED_TIBIA_VERSION;
				}
				Tibia::SetHost(Version, HostLen, Host, Port);
				Tibia::SetVersionString(Version);
			}
			else {
				if (Tibia::Version != Version) {
					return ERROR_WRONG_VERSION;
				}
				if (Tibia::HostLen != HostLen || DiffMemory(Tibia::Host, Host, HostLen)) {
					return ERROR_WRONG_HOST;
				}
			}
		}
		return NULL;
	}

	VOID FillSessionList() {
		Current = First;
		do {
			ListBox_SetItemData(MainWnd::ListSessions, ListBox_AddString(MainWnd::ListSessions, TimeStr::Set(ListBox_GetCount(MainWnd::ListSessions), CurrentLogin->SessionTime(), CurrentLogin->Last->Time)), Current);
		} while (Current = CurrentLogin->Last->Next);
	}
	VOID LastTimeChanged() {
		MainWnd::ScrollInfo.nMax = Last->Time / 1000 + 59;
	}
	VOID AfterOpen(BOOL Override) {
		Current->EndSession();
		if (Last) {
			INT NewSession = ListBox_GetCount(MainWnd::ListSessions);
			SetWindowRedraw(MainWnd::ListSessions, FALSE);
			while (Current = Last->Next) {
				ListBox_SetItemData(MainWnd::ListSessions, ListBox_AddString(MainWnd::ListSessions, TimeStr::Set(ListBox_GetCount(MainWnd::ListSessions), CurrentLogin->SessionTime(), CurrentLogin->Last->Time)), Current);
				Last = CurrentLogin->Last;
			}
			SetWindowRedraw(MainWnd::ListSessions, TRUE);
			ListBox_SetCurSel(MainWnd::ListSessions, NewSession);
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
			Changed = TRUE;
		}
		else {
			Last = Current;
			Tibia::OpenVersionMenu();
			SetWindowRedraw(MainWnd::ListSessions, FALSE);
			FillSessionList();
			SetWindowRedraw(MainWnd::ListSessions, TRUE);
			ListBox_SetCurSel(MainWnd::ListSessions, 0);
			TCHAR LabelString[40];
			LoadString(NULL, LABEL_TOTAL_TIME, LabelString, 40);
			Static_SetText(MainWnd::LabelTime, LabelString);
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
			LoadString(NULL, BUTTON_SAVE, LabelString, 40);
			Button_SetText(MainWnd::ButtonSub, LabelString);
			Changed = Override;
		}
		LastTimeChanged();
	}

	Packet *&Started() {
		return Last ? Last->Next : First;
	}
	VOID CancelOpen(CONST BOOL Override) {
		if (Packet *&Start = Started()) {
			if (Override) {
				AfterOpen(TRUE);
			}
			else {
				Current = Start;
				Start = NULL;
				Unload();
			}
		}
	}

	struct FilePacket : private NeedParser, PacketBase {
		BOOL Read(BufferedFile &File) {
			Set(File.Skip(2));
			if (!P || !P->Size || !File.Skip(P->Size)) {
				return FALSE;
			}
			Parser->SetPacket(*this);
			return Parser->GetPacketType();
		}
		BOOL Record(Packet *&Next) {
			if (Next) {
				if (LPBYTE Data = Parser->AllocPacket(*Next, P->Size)) {
					CopyMemory(Data, P->Data, P->Size);
					return TRUE;
				}
				delete Next;
				Next = NULL;
			}
			return FALSE;
		}
	};

	UINT Open(BOOL Override, CONST HWND Parent) {
		BufferedFile File;  // faster than ReadingFile, uses more memory (should we have a SafeOpen for big files?)
		if (!File.Open(FileName, OPEN_EXISTING)) {
			return ERROR_CANNOT_OPEN_VIDEO_FILE;
		}
		WORD Version;
		if (!File.ReadWord(Version)) {
			return ERROR_CORRUPT_VIDEO;
		}
		BYTE HostLen;
		if (!File.ReadByte(HostLen) || HostLen > 127) {
			return ERROR_CORRUPT_VIDEO;
		}
		LPCSTR Host = NULL; WORD Port = PORT;
		if (HostLen) {
			Host = LPCSTR(File.Skip(HostLen));
			if (!Host || !Tibia::VerifyHost(Host, HostLen)) {
				return ERROR_CORRUPT_VIDEO;
			}
			if (!File.ReadWord(Port) || !Port) {
				return ERROR_CORRUPT_VIDEO;
			}
		}
		if (UINT Error = BeforeOpen(Override, Parent, Version, HostLen, Host, Port)) {
			return Error;
		}
		DWORD TotalTime;
		if (!File.ReadDword(TotalTime)) {
			return ERROR_CORRUPT_VIDEO;
		}
		FilePacket Src;
		if (!Src.Read(File) || !Parser->PlayerData) {
			return ERROR_CORRUPT_VIDEO;
		}
		if (!Parser->EnterGame) { // videos recorded with TTM BETA between 9.80 and 10.11 may have this buggy packet: fix them
			if (!Parser->Pending || !Src.Read(File) || !Parser->EnterGame || Parser->PlayerData) {
				return ERROR_CORRUPT_VIDEO;
			}
		}
		if (Last) {
			if (TotalTime > INFINITE - 1000 || TotalTime + 1000 > INFINITE - Last->Time) {
				return ERROR_CANNOT_APPEND;
			}
			TotalTime += Last->Time + 1000;
			if (!Src.Record(Last->Next = new(std::nothrow) Session(Last))) {
				return ERROR_CANNOT_OPEN_VIDEO_FILE;
			}
			Current = Last->Next;
		}
		else {
			if (!Src.Record(First = new(std::nothrow) Session())) {
				return ERROR_CANNOT_OPEN_VIDEO_FILE;
			}
			Current = First;
		}
		if (!Parser->FixEnterGame(*Current)) {
			CancelOpen(Override);
			return ERROR_CANNOT_OPEN_VIDEO_FILE;
		}
		for (BYTE EnterGame; File.ReadByte(EnterGame); Current = Current->Next) {
			MainWnd::Progress_Set(Current->Time, TotalTime);
			if (!EnterGame) {
				WORD Delay;
				if (!File.ReadWord(Delay) || Delay > TotalTime - Current->Time) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				if (!Src.Read(File) || Parser->EnterGame || Parser->Pending || Parser->PlayerData) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				if (!Src.Record(Current->Next = new(std::nothrow) Packet(Current, Delay))) {
					CancelOpen(Override);
					return ERROR_CANNOT_OPEN_VIDEO_FILE;
				}
				if (!Parser->FixTrade(*Current->Next)) {
					CancelOpen(Override);
					return ERROR_CANNOT_OPEN_VIDEO_FILE;
				}
			}
			else if (EnterGame == TRUE) {
				if (1000 > TotalTime - Current->Time) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				Current->EndSession();
				if (!Src.Read(File) || !Parser->EnterGame) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				if (!Src.Record(Current->Next = new(std::nothrow) Session(Current))) {
					CancelOpen(Override);
					return ERROR_CANNOT_OPEN_VIDEO_FILE;
				}
				if (!Parser->FixEnterGame(*Current->Next)) {
					CancelOpen(Override);
					return ERROR_CANNOT_OPEN_VIDEO_FILE;
				}
			}
			else {
				CancelOpen(Override);
				return ERROR_CORRUPT_VIDEO;
			}
		}
		if (Current->Time != TotalTime) {
			CancelOpen(Override);
			return ERROR_CORRUPT_VIDEO;
		}
		AfterOpen(Override);
		return NULL;
	}

	class Converter : private NeedParser, PacketBase {
		WORD PacketSize;
		WORD Want;
		LPBYTE Store;
		DWORD LastTime;
	public:
		DWORD Time;

		Converter(): Store(LPBYTE(&PacketSize)), Want(2) {
			Current = Last;
		}
		~Converter() {
			delete[] LPBYTE(P);
		}
		BOOL Read(LPBYTE Data, DWORD Avail) {
			while (Avail >= Want) {
				CopyMemory(Store, Data, Want);
				Data += Want;
				Avail -= Want;
				if (!P) {
					if (PacketSize) {
						Store = Parser->AllocPacket(*this, PacketSize);
						if (!Store) {
							Time = ERROR_CANNOT_OPEN_VIDEO_FILE;
							return FALSE;
						}
						Want = PacketSize;
					}
					else {
						Store = LPBYTE(&PacketSize);
						Want = 2;
					}
				}
				else {
					if (!Parser->GetPacketType()) {
						Time = ERROR_CORRUPT_VIDEO;
						return FALSE;
					}
					if (Parser->EnterGame) {
						if (!Parser->FixEnterGame(*this)) {
							Time = ERROR_CANNOT_OPEN_VIDEO_FILE;
							return FALSE;
						}
						if (Current) {
							if (1000 > INFINITE - Current->Time) {
								Time = ERROR_CANNOT_APPEND;
								return FALSE;
							}
							Current->EndSession();
							if (!(Current = Current->Next = new(std::nothrow) Session(Current))) {
								Time = ERROR_CANNOT_OPEN_VIDEO_FILE;
								return FALSE;
							}
						}
						else {
							if (!(Current = First = new(std::nothrow) Session())) {
								Time = ERROR_CANNOT_OPEN_VIDEO_FILE;
								return FALSE;
							}
						}
						Current->Record(*this);
					}
					else if (Parser->Pending || Parser->PlayerData) {
						Discard(); // pending packet, just get data (should not exist, but who knwows)
					}
					else {
						if (!Started())  {
							Time = ERROR_CORRUPT_VIDEO;
							return FALSE; // common packet without a login packet first (usually wrong version selected)
						}
						if (!Parser->FixTrade(*this)) {
							Time = ERROR_CANNOT_OPEN_VIDEO_FILE;
							return FALSE;
						}
						DWORD Delay = Time - LastTime;
						if (Delay > BIGDELAY) {
							Delay = BIGDELAY;
						}
						if (Delay > INFINITE - Current->Time) {
							Delay = INFINITE - Current->Time;
						}
						if (!(Current = Current->Next = new(std::nothrow) Packet(Current, WORD(Delay)))) {
							Time = ERROR_CANNOT_OPEN_VIDEO_FILE;
							return FALSE;
						}
						Current->Record(*this);
					}
					LastTime = Time;
					Store = LPBYTE(&PacketSize);
					Want = 2;
				}
			}
			if (Avail) {
				CopyMemory(Store, Data, Avail);
				Store += Avail;
				Want -= Avail;
			}
			return TRUE;
		}
	};

	BYTE RECVersion() {
		return Tibia::Version >= 1080 ? 11 : Tibia::Version >= 1058 ? 10 : Tibia::Version >= 1054 ? 9 : Tibia::Version >= 980 ? 8 : Tibia::Version >= 830 ? 7 : Tibia::Version >= 800 ? 6 : Tibia::Version >= 772 ? 5 : Tibia::Version >= 770 ? 4 : Tibia::Version >= 710 ? 3 : 2;
	}
	INT CAMProgressCallback(LPVOID This, QWORD DecSize, QWORD EncSize, QWORD TotalSize) { //Provided by my custom LzmaLib
		MainWnd::Progress_Set(DecSize, TotalSize);
		return 0;
	}
	UINT SaveCAM() {
		NeedParser ToSave;
		DWORD Size = Parser->GetPacketData(*First)->RawSize() + 16, Packets = 58;
		for (Current = First; Current = Current->Next; Packets++) {
			if (Packets == INFINITE) {
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			if ((Size += Parser->GetPacketData(*Current)->RawSize() + 10) > 0x7FFFFFA2) {
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			MainWnd::Progress_Set(Current->Time, Last->Time);
		}
		LzmaFile File;
		if (!File.StartHeader(Size, Tibia::HostLen ? Tibia::HostLen + 43 : 40)) {
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		File.Write(CAM_HASH, 32); // Our little mod to allow otserver info, no other player checks the hash
		File.WriteByte((Tibia::Version / 100) % 100);
		File.WriteByte((Tibia::Version / 10) % 10);
		File.WriteByte(Tibia::Version % 10);
		File.WriteByte(0);
		if (Tibia::HostLen) {
			File.WriteDword(Tibia::HostLen + 3);
			File.WriteByte(Tibia::HostLen);
			File.Write(Tibia::Host, Tibia::HostLen);
			File.WriteWord(Tibia::Port);
		}
		else {
			File.WriteDword(0);
		}
		File.EndHeader();
		File.WriteByte(RECVersion()); // Ignored by all players
		File.WriteByte(2); // It mimics an encrypted REC file, but without encryption
		File.WriteDword(Packets);
		Current = First;
		do {
			PacketData* Packet = Parser->GetPacketData(*Current);
			if ((Size = Packet->RawSize()) > 0xFFFF) {
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			File.WriteWord(Size);
			File.WriteDword(Current->Time);
			File.Write(Packet, Size);
			File.WriteDword(crc32(0, Packet->Data, Packet->Size));
			MainWnd::Progress_Set(Current->Time, Last->Time);
		} while (Current = Current->Next);
		if (!File.Compress(CAMProgressCallback)) {
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		MainWnd::Progress_Start();
		if (!File.Save(FileName, CREATE_ALWAYS)) {
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		Changed = FALSE;
		return NULL;
	}
	UINT OpenCAM(BOOL Override, CONST HWND Parent) {
		LzmaFile File;
		if (!File.Open(FileName, OPEN_EXISTING)) {
			return ERROR_CANNOT_OPEN_VIDEO_FILE;
		}
		LPBYTE Hash = File.Skip(32); // No recorder uses this as a real hash
		if (!Hash) {
			return ERROR_CORRUPT_VIDEO;
		}
		LPBYTE VersionPart = File.Skip(4);
		if (!VersionPart || VersionPart[0] > 99 || VersionPart[1] > 9 || VersionPart[2] > 9 || VersionPart[3]) {
			return ERROR_CORRUPT_VIDEO;
		}
		WORD Version = VersionPart[0] * 100 + VersionPart[1] * 10 + VersionPart[2];
		BYTE HostLen = NULL;
		LPCSTR Host = NULL;
		WORD Port = PORT;
		DWORD Metadata;
		if (!File.ReadDword(Metadata)) {
			return ERROR_CORRUPT_VIDEO;
		}
		if (Metadata) {
			if (!DiffMemory(Hash, CAM_HASH, 32)) { // Our little mod to allow otserver info
				if (Metadata < 4 || Metadata > 130) {
					return ERROR_CORRUPT_VIDEO;
				}
				if (!File.ReadByte(HostLen) || HostLen != Metadata - 3) {
					return ERROR_CORRUPT_VIDEO;
				}
				Host = LPCSTR(File.Skip(HostLen));
				if (!Host || !Tibia::VerifyHost(Host, HostLen)) {
					return ERROR_CORRUPT_VIDEO;
				}
				if (!File.ReadWord(Port) || !Port) {
					return ERROR_CORRUPT_VIDEO;
				}
			}
			else if (!File.Skip(Metadata)) {
				return ERROR_CORRUPT_VIDEO;
			}
		}
		if (UINT Error = BeforeOpen(Override, Parent, Version, HostLen, Host, Port)) {
			return Error;
		}
		if (!File.Uncompress(Override)) {
			return ERROR_CORRUPT_VIDEO;
		}
		BYTE RecVersion;
		if (!File.ReadByte(RecVersion)) { // Fake TibiCAM version, ignore it (all other CAM recorders use 6 because >822)
			return ERROR_CORRUPT_VIDEO;
		}
		if (!File.ReadByte(RecVersion) || RecVersion != 2) { // Fake TibiCAM encryption flag (but not really encrypted)
			return ERROR_CORRUPT_VIDEO;
		}
		DWORD Packets;
		if (!File.ReadDword(Packets) || Packets < 58) {
			return ERROR_CORRUPT_VIDEO;
		}
		Packets -= 57;
		Converter Src; // Shortcut to read all kinds of videos, could use FilePacket
		for (DWORD i = 0; i < Packets; i++) {
			WORD Size;
			if (!File.ReadWord(Size)) {
				CancelOpen(Override);
				return ERROR_CORRUPT_VIDEO;
			}
			if (!File.ReadDword(Src.Time)) {
				CancelOpen(Override);
				return ERROR_CORRUPT_VIDEO;
			}
			LPBYTE Data = File.Skip(DWORD(Size) + 4); // Ignore checksum, some recorders misuse it (LZMA already checksums)
			if (!Data) {
				CancelOpen(Override);
				return ERROR_CORRUPT_VIDEO;
			}
			if (!Src.Read(Data, Size)) {
				CancelOpen(Override);
				return Src.Time;
			}
			MainWnd::Progress_Set(i, Packets);
		}
		if (!Started()) {
			return ERROR_CORRUPT_VIDEO;
		}
		AfterOpen(Override);
		return NULL;
	}

	UINT SaveTMV() {
		DeflateFile File;
		if (!File.Open(FileName, CREATE_ALWAYS)) {
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.WriteWord(2)) { // Tibiamovie file version (ignored by original player)
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.WriteWord(Tibia::Version)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.WriteDword(Last->Time)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.WriteByte(FALSE)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.WriteDword(0)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		NeedParser ToSave;
		PacketData* Packet = Parser->GetPacketData(*(Current = First));
		DWORD Size;
		if ((Size = Packet->RawSize()) > 0xFFFF) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.WriteWord(Size)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.Write(Packet, Size)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		while (Current->Next) {
			if (Current->IsLast()) {
				if (!File.WriteByte(TRUE)) { // I'm adding markers to the ends of the sessions
					File.Delete(FileName);
					return ERROR_CANNOT_SAVE_VIDEO_FILE;
				}
			}
			if (!File.WriteByte(FALSE)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			if (!File.WriteDword(Current->Next->Time - Current->Time)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			MainWnd::Progress_Set(Current->Time, Last->Time);
			Packet = Parser->GetPacketData(*(Current = Current->Next));
			if ((Size = Packet->RawSize()) > 0xFFFF) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			if (!File.WriteWord(Size)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			if (!File.Write(Packet, Size)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
		}
		MainWnd::Progress_Start();
		if (!File.Save()) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		Changed = FALSE;
		return NULL;
	}
	UINT OpenTMV(BOOL Override, CONST HWND Parent) {
		InflateFile File;
		if (!File.Open(FileName, OPEN_EXISTING)) {
			return ERROR_CANNOT_OPEN_VIDEO_FILE;
		}
		WORD Version;
		if (!File.ReadWord(Version) || Version != 2) {
			return ERROR_CORRUPT_VIDEO;
		}
		if (!File.ReadWord(Version)) {
			return ERROR_CORRUPT_VIDEO;
		}
		DWORD TotalTime;
		if (!File.ReadDword(TotalTime)) {
			return ERROR_CORRUPT_VIDEO;
		}
		if (UINT Error = BeforeOpen(Override, Parent, Version, NULL, NULL, PORT)) {
			return Error;
		}
		Converter Src;
		Src.Time = 0;
		for (BYTE Marker; File.ReadByte(Marker); MainWnd::Progress_Set(Src.Time, TotalTime)) {
			if (!Marker) {
				DWORD Delay;
				if (!File.ReadDword(Delay) || !Override && Delay > TotalTime - Src.Time) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				Src.Time += Delay;
				WORD Size;
				if (!File.ReadWord(Size)) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				if (Size) { // TMV adds zero-sized packets to delay between merged videos
					BYTE Data[0xFFFF];
					if (!File.Read(Data, Size)) {
						CancelOpen(Override);
						return ERROR_CORRUPT_VIDEO;
					}
					if (!Src.Read(Data, Size)) {
						CancelOpen(Override);
						return Src.Time;
					}
				}
			}
			else if (Marker != TRUE) {
				CancelOpen(Override);
				return ERROR_CORRUPT_VIDEO;
			}
		}
		if (!Started()) {
			return ERROR_CORRUPT_VIDEO;
		}
		if (Src.Time != TotalTime) {
			CancelOpen(Override);
			return ERROR_CORRUPT_VIDEO;
		}
		AfterOpen(Override);
		return NULL;
	}

	UINT SaveREC() {
		NeedParser ToSave;
		DWORD Size = Parser->GetPacketData(*First)->RawSize() + 14, Packets = 1;
		for (Current = First; Current = Current->Next; Packets++) {
			if (Packets == INFINITE) {
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			if ((Size += Parser->GetPacketData(*Current)->RawSize() + 8) > 0xFFFEFFFE) {
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			MainWnd::Progress_Set(Current->Time, Last->Time);
		}
		BufferedFile File; // could have been WritingFile, but this is faster and we already looped trough the packets
		if (!File.Start(Size)) {
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		File.WriteByte(RECVersion()); // this version control is what made me create ttm
		File.WriteByte(1); // there is no point in saving encrypted rec files anymore, and they are slower
		File.WriteDword(Packets);
		Current = First;
		do {
			PacketData* Packet = Parser->GetPacketData(*Current);
			File.WriteDword(Size = Packet->RawSize());
			File.WriteDword(Current->Time);
			File.Write(Packet, Size);
			MainWnd::Progress_Set(Current->Time, Last->Time);
		} while (Current = Current->Next);
		if (!File.Save(FileName, CREATE_ALWAYS)) {
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		Changed = FALSE;
		return NULL;
	}
	WORD GuessVersion(CONST BYTE RecVersion, CONST BYTE Encryption) {
		switch (RecVersion) { //TODO: guess version by packet contents
			case 2: return 700; // never found one, let's use for older-than-tibicam recordings
			case 3:	switch (Encryption) {
				case 1:	return 710; // no encryption
				case 2: return 730; // encryption mod 5
			}
			case 4: return 770; // encryption mod 8
			case 5: return 772; // encryption mod 8 + aes
			case 6: return 800; // encryption mod 6 + aes
			case 7: return 830; // versions that tibicam never supported
			case 8: return 980; // when the enter game packet changed
			case 9: return 1054;
			case 10: return 1058;
			case 11: return 1080;
		}
		return LATEST; //never happens
	}
	UINT OpenREC(BOOL Override, CONST HWND Parent) {
		BufferedFile File; // faster than ReadingFile, uses more memory
		if (!File.Open(FileName, OPEN_EXISTING)) {
			return ERROR_CANNOT_OPEN_VIDEO_FILE;
		}
		BYTE RecVersion;
		if (!File.ReadByte(RecVersion) || RecVersion < 2) { // we are supporting more versions than tibicam itself
			return ERROR_CORRUPT_VIDEO;
		}
		BYTE Encryption;
		if (!File.ReadByte(Encryption) || !Encryption || Encryption > 2) {
			return ERROR_CORRUPT_VIDEO;
		}
		if (!Last && !Tibia::Running) {
			Tibia::SetHost(GuessVersion(RecVersion, Encryption), NULL, LPCTSTR(NULL), PORT);
			MainWnd::Progress_Pause();
			if (!Loader().Run(Parent, TITLE_LOADER_OVERRIDE)) {
				return ERROR_TIBICAM_VERSION;
			}
			MainWnd::Progress_Start();
		}
		DWORD Packets;
		Converter Src;
		if (Encryption == 2) {
			if (!File.ReadDword(Packets) || Packets < 58) {
				return ERROR_CORRUPT_VIDEO;
			}
			Packets -= 57;
			CHAR Mod = RecVersion < 4 ? 5 : RecVersion < 6 ? 8 : 6;
			if (RecVersion > 4 && !RecKey) {
				struct AES256KEYBLOB {
					BLOBHEADER Header = { PLAINTEXTKEYBLOB, CUR_BLOB_VERSION, NULL, CALG_AES_256 };
					DWORD Size = 32;
					BYTE Key[33] = "Thy key is mine © 2006 GB Monaco";
				} AesBlob;
				if (!CryptImportKey(WinCrypt, LPBYTE(&AesBlob), sizeof(AesBlob), NULL, NULL, &RecKey)) {
					return ERROR_CANNOT_OPEN_VIDEO_FILE;
				}
				DWORD AesMode = CRYPT_MODE_ECB;
				if (!CryptSetKeyParam(RecKey, KP_MODE, LPBYTE(&AesMode), NULL)) {
					CryptDestroyKey(RecKey);
					RecKey = NULL;
					return ERROR_CANNOT_OPEN_VIDEO_FILE;
				}
			}
			for (DWORD i = 0; i < Packets; i++) {
				WORD Size;
				if (!File.ReadWord(Size)) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				if (!File.ReadDword(Src.Time)) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				LPBYTE Data = File.Skip(Size);
				if (!Data) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				DWORD Checksum;
				if (!File.ReadDword(Checksum) || !Override && Checksum != adler32(1, Data, Size)) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				CHAR Key = Size + Src.Time - 31, Rem;
				for (WORD i = 0; i < Size; i++) {
					if ((Rem = (Key += 33) % Mod) > 0) {
						Rem -= Mod;
					}
					Data[i] -= Key - Rem;
				}
				if (RecVersion > 4 && Size) {
					DWORD AesSize = Size;
					if (!CryptDecrypt(RecKey, NULL, TRUE, NULL, Data, &AesSize) || !AesSize) {
						CancelOpen(Override);
						return ERROR_CORRUPT_VIDEO;
					}
					Size = WORD(AesSize);
				}
				if (!Src.Read(Data, Size)) {
					CancelOpen(Override);
					return Src.Time;
				}
				MainWnd::Progress_Set(i, Packets);
			}
		}
		else {
			if (!File.ReadDword(Packets) || !Packets) {
				return ERROR_CORRUPT_VIDEO;
			}
			for (DWORD i = 0; i < Packets; i++) {
				DWORD Size;
				if (!File.ReadDword(Size)) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				if (!File.ReadDword(Src.Time)) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				LPBYTE Data = File.Skip(Size);
				if (!Data) {
					CancelOpen(Override);
					return ERROR_CORRUPT_VIDEO;
				}
				if (!Src.Read(Data, Size)) {
					CancelOpen(Override);
					return Src.Time;
				}
				MainWnd::Progress_Set(i, Packets);
			}
		}
		if (!Started()) {
			return ERROR_CORRUPT_VIDEO;
		}
		AfterOpen(Override);
		return NULL;
	}

	DWORD DetectFormat() {
		if (LPCTSTR Extension = PathFindExtension(FileName)) {
			if (!_tcsicmp(Extension, _T(".ttm"))) {
				return FILETYPE_TTM;
			}
			if (!_tcsicmp(Extension, _T(".cam"))) {
				return FILETYPE_CAM;
			}
			if (!_tcsicmp(Extension, _T(".tmv"))) {
				return FILETYPE_TMV;
			}
			if (!_tcsicmp(Extension, _T(".rec"))) {
				return FILETYPE_REC;
			}
		}
		return NULL;
	}

	UINT SaveMultiple(CONST DWORD Format, CONST BOOL Override, CONST HWND Parent) {
		switch (Format) {
		case NULL:
			break;
		case FILETYPE_TTM:
			return Save();
		case FILETYPE_CAM:
			return SaveCAM();
		case FILETYPE_TMV:
			return SaveTMV();
		case FILETYPE_REC:
			return SaveREC();
		case FILETYPE_ALL:
			return SaveMultiple(DetectFormat(), Override, Parent);
		}
		return ERROR_VIDEO_FORMAT_NOT_SUPPORTED;
	}
	UINT OpenMultiple(CONST DWORD Format, CONST BOOL Override, CONST HWND Parent) {
		switch (Format) {
		case NULL:
			break;
		case FILETYPE_TTM:
			return Open(Override, Parent);
		case FILETYPE_CAM:
			return OpenCAM(Override, Parent);
		case FILETYPE_TMV:
			return OpenTMV(Override, Parent);
		case FILETYPE_REC:
			return OpenREC(Override, Parent);
		case FILETYPE_ALL:
			return OpenMultiple(DetectFormat(), Override, Parent);
		}
		return ERROR_VIDEO_FORMAT_NOT_SUPPORTED;
	}

	VOID OpenCmd(CONST LPCTSTR CmdLine) {
		Timer::Setup();
		CONST SIZE_T CmdLineLen = _tcslen(CmdLine);
		if (!CmdLineLen) {
			return;
		}
		MainWnd::Wait();
		if (CmdLineLen >= MAX_PATH) {
			return ErrorBox(ERROR_CMDLINE, TITLE_OPEN_VIDEO);
		}
		TCHAR File[MAX_PATH];
		CopyMemory(File, CmdLine, TLEN(CmdLineLen + 1));
		PathRemoveBlanks(File);
		PathRemoveArgs(File);
		PathUnquoteSpaces(File);
		if (!DiffMemory(File, _T("otserv:"), TLEN(7))) {
			return Tibia::StartURL(File);
		}
		GetFullPathName(File, MAX_PATH, FileName, NULL);
		if (!PathFileExists(FileName)) {
			FileName[0] = NULL;
			return ErrorBox(ERROR_FILE_NOT_EXISTS, TITLE_OPEN_VIDEO);
		}
		MainWnd::Progress_Start();
		if (CONST UINT Error = OpenMultiple(DetectFormat(), GetKeyState(VK_SHIFT) < 0, MainWnd::Handle)) {
			ErrorBox(Error, TITLE_OPEN_VIDEO);
		} //TODO: open exe, detect custom client and import accordingly
		if (Last) {
			MainWnd::Done();
			SetFileTitle();
			Tibia::AutoPlay();
			MainWnd::Progress_Stop();
		}
	}
	VOID OpenDrop(CONST HDROP Drop) {
		BOOL Override = GetKeyState(VK_SHIFT) < 0;
		MainWnd::Wait();
		MainWnd::Progress_Start();
		INT SessionNumber = ListBox_GetCount(MainWnd::ListSessions);
		TCHAR FirstName[MAX_PATH];
		CopyMemory(FirstName, FileName, TLEN(MAX_PATH));
		TCHAR ErrorString[900];
		SIZE_T Pos = 0;
		MainWnd::Progress_Segments = DragQueryFile(Drop, INFINITE, NULL, NULL);
		for (UINT i = 0; DragQueryFile(Drop, i, FileName, MAX_PATH); i++) {
			if (!Last) {
				CopyMemory(FirstName, FileName, TLEN(MAX_PATH));
			}
			MainWnd::Progress_Segment = i;
			if (CONST UINT Error = OpenMultiple(DetectFormat(), Override, MainWnd::Handle)) {
				LPCTSTR ErrorFile = PathFindFileName(FileName);
				SIZE_T FileSize = _tcslen(ErrorFile);
				if (Pos) {
					if (Pos + FileSize + 4 + RSTRING(Error).Len > 900) {
						continue;
					}
					ErrorString[Pos++] = '\n';
					ErrorString[Pos++] = '\n';
				}
				CopyMemory(ErrorString + Pos, ErrorFile, TLEN(FileSize));
				ErrorString[(Pos += FileSize)++] = '\n';
				Pos += LoadString(NULL, Error, ErrorString + Pos, 900 - Pos);
				if (Error == ERROR_TIBICAM_VERSION) {
					break;
				}
			}
			else {
				SHAddToRecentDocs(SHARD_PATH, FileName);
			}
		}
		MainWnd::Progress_Segment = 0;
		MainWnd::Progress_Segments = 1;
		MainWnd::Done();
		CopyMemory(FileName, FirstName, TLEN(MAX_PATH));
		if (Pos) {
			MainWnd::Progress_Error();
			TCHAR TitleString[50];
			LoadString(NULL, TITLE_OPEN_VIDEO, TitleString, 50);
			MessageBox(MainWnd::Handle, ErrorString, TitleString, MB_ICONSTOP);
		}
		if (Last) {
			if (ListBox_GetCount(MainWnd::ListSessions) > SessionNumber) {
				SetFileTitle();
				ListBox_SetCurSel(MainWnd::ListSessions, SessionNumber);
				Tibia::AutoPlay();
			}
		}
		MainWnd::Progress_Stop();
	}

	UINT_PTR CALLBACK FileDialogHook(HWND Dialog, UINT Message, WPARAM Wp, LPARAM Lp) {
		switch (Message) {
			case WM_NOTIFY:
				switch (LPNMHDR(Lp)->code) {
					case CDN_INITDONE:
						if (!Last) {
							HWND Parent = GetParent(Dialog);
							TCHAR Overrride[40];
							LoadString(NULL, LABEL_OVERRIDE, Overrride, 40);
							CommDlg_OpenSave_SetControlText(Parent, chx1, Overrride);
						}
						break;
					case CDN_FILEOK:
						MainWnd::Progress_Start();
						CONST LPOPENFILENAME &Info = LPOFNOTIFY(Lp)->lpOFN;
						if (CONST UINT Error = ((UINT(*)(CONST DWORD, CONST BOOL, CONST HWND))Info->lCustData)(Info->nFilterIndex, Info->Flags & OFN_READONLY, GetParent(Dialog))) {
							MainWnd::Progress_Error();
							TCHAR ErrorString[200];
							LoadString(NULL, Error, ErrorString, 200);
							MessageBox(Dialog, ErrorString, Info->lpstrTitle, MB_ICONSTOP);
							MainWnd::Progress_Stop();
							if (!Last) {
								SetWindowLongPtr(Dialog, DWLP_MSGRESULT, TRUE);
								return TRUE;
							}
						}
						SetFileTitle();
						break;
				}
				break;
		}
		return FALSE;
	}

	VOID FileDialog() {
		TCHAR Formats[300];
		SIZE_T Pos;
		CopyMemory(Formats + (Pos = LoadString(NULL, FILETYPE_TTM, Formats, 214)), _T(" (*.ttm)\0*.ttm"), TLEN(15));
		CopyMemory(Formats + (Pos += LoadString(NULL, FILETYPE_CAM, Formats + (Pos += 15), 214 - Pos)), _T(" (*.cam)\0*.cam"), TLEN(15));
		CopyMemory(Formats + (Pos += LoadString(NULL, FILETYPE_TMV, Formats + (Pos += 15), 214 - Pos)), _T(" (*.tmv)\0*.tmv"), TLEN(15));
		CopyMemory(Formats + (Pos += LoadString(NULL, FILETYPE_REC, Formats + (Pos += 15), 214 - Pos)), _T(" (*.rec)\0*.rec"), TLEN(15));
		CopyMemory(Formats + (Pos += LoadString(NULL, FILETYPE_ALL, Formats + (Pos += 15), 214 - Pos)), _T("\0*.ttm;*.cam;*.tmv;*.rec"), TLEN(25));
		Formats[Pos + 25] = NULL;
		TCHAR Title[20];
		OPENFILENAME OpenFileName;
		OpenFileName.lStructSize = sizeof(OPENFILENAME);
		OpenFileName.hwndOwner = MainWnd::Handle;
		OpenFileName.lpstrTitle = Title;
		OpenFileName.lpstrFilter = Formats;
		OpenFileName.lpstrCustomFilter = NULL;
		OpenFileName.lpstrFile = FileName;
		OpenFileName.nMaxFile = MAX_PATH;
		OpenFileName.lpstrFileTitle = NULL;
		OpenFileName.lpstrInitialDir = NULL;
		OpenFileName.lpstrDefExt = _T("ttm");
		OpenFileName.lpfnHook = FileDialogHook;
		OpenFileName.FlagsEx = 0;
		if (Last) {
			OpenFileName.nFilterIndex = DetectFormat();
			OpenFileName.lCustData = LPARAM(SaveMultiple);
			OpenFileName.Flags = OFN_ENABLESIZING | OFN_EXPLORER | OFN_ENABLEHOOK | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOTESTFILECREATE | OFN_PATHMUSTEXIST;
			LoadString(NULL, TITLE_SAVE_VIDEO, Title, 20);
			GetSaveFileName(&OpenFileName);
		}
		else {
			OpenFileName.nFilterIndex = FILETYPE_ALL;
			OpenFileName.lCustData = LPARAM(OpenMultiple);
			OpenFileName.Flags = OFN_ENABLESIZING | OFN_EXPLORER | OFN_ENABLEHOOK | OFN_FILEMUSTEXIST;
			LoadString(NULL, TITLE_OPEN_VIDEO, Title, 20);
			if (GetOpenFileName(&OpenFileName)) {
				Tibia::AutoPlay();
			}
		}
		MainWnd::Progress_Stop();
	}

	VOID SaveChanges() {
		if (Changed) {
			FileDialog();
		}
	}
	BOOL GetDesktop(CONST LPCTSTR Append) {
		if (SHGetSpecialFolderPath(NULL, FileName, CSIDL_DESKTOPDIRECTORY, TRUE)) {
			PathAppend(FileName, Append);
			return TRUE;
		}
		GetFullPathName(Append, MAX_PATH, FileName, NULL);
		return FALSE;
	}
	VOID SaveRecovery() {
		if (Changed) {
			MainWnd::Progress_Start();
			if (GetDesktop(_T("TTM Recovery.ttm"))) {
				if (Save()) {
					MessageBeep(MB_ICONSTOP);
				}
			}
		}
	}

	INT GetSession() {
		CONST INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
		if (LoginNumber < 0) {
			Current = First;
			return 0;
		}
		Current = (Packet*) ListBox_GetItemData(MainWnd::ListSessions, LoginNumber);
		return LoginNumber;
	}

	VOID SessionTimeChanged(CONST INT LoginNumber) {
		LastTimeChanged();
		SetWindowRedraw(MainWnd::ListSessions, FALSE);
		ListBox_ResetContent(MainWnd::ListSessions);
		FillSessionList();
		SetWindowRedraw(MainWnd::ListSessions, TRUE);
		ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
	}
	VOID AfterClose() {
		Changed = FALSE;
		TimeStr::SetTimeSeconds(0);
		ClearFileTitle();
		Tibia::CloseVersionMenu();
		ListBox_ResetContent(MainWnd::ListSessions);
	}
	Session *UnloadSession(CONST INT LoginNumber) {
		if (Session *&NextLogin = *(Session**) &((CurrentLogin->Prev ? CurrentLogin->Prev->Next : First) = CurrentLogin->Last->Next)) {
			NextLogin->Prev = CurrentLogin->Prev;
			CurrentLogin->Last->Next = NULL;
			PlayedTime = NextLogin->Time - Current->Time;
			Unload();
			Current = NextLogin;
			do {
				Current->Time -= PlayedTime;
			} while (Current = Current->Next);
			Changed = TRUE;
			SessionTimeChanged(LoginNumber);
			return NextLogin;
		}
		Last = CurrentLogin->Prev;
		Unload();
		if (Last) {
			Changed = TRUE;
			SessionTimeChanged(LoginNumber - 1);
			return Last->Login;
		}
		AfterClose();
		return NULL;
	}

	VOID UnloadClose() {
		Current = First;
		First = NULL;
		Last = NULL;
		Unload();
		AfterClose();
	}

	VOID Close() {
		if (Last) {
			UnloadClose();
			TCHAR LabelString[40];
			LoadString(NULL, LABEL_NO_VIDEO, LabelString, 40);
			Static_SetText(MainWnd::LabelTime, LabelString);
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
			LoadString(NULL, BUTTON_OPEN, LabelString, 40);
			Button_SetText(MainWnd::ButtonSub, LabelString);
		}
	}
	VOID Delete() {
		if (Last) {
			if (!UnloadSession(GetSession())) {
				TCHAR LabelString[40];
				LoadString(NULL, LABEL_NO_VIDEO, LabelString, 40);
				Static_SetText(MainWnd::LabelTime, LabelString);
				LoadString(NULL, BUTTON_OPEN, LabelString, 40);
				Button_SetText(MainWnd::ButtonSub, LabelString);
			}
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
		}
	}

	VOID Start() {
		if (!Tibia::Running) {
			if (!Video::Last) {
				if (!Loader().Run(MainWnd::Handle, TITLE_LOADER)) {
					return;
				}
			}
			Tibia::Start();
			if (!Tibia::Running) {
				return;
			}
		}
		else if (Proxy::State == Proxy::GAME_PLAY) {
			Proxy::HandleReconnect();
		}
		Tibia::Flash();
		State = WAIT;
		TCHAR LabelString[40];
		LoadString(NULL, LABEL_WAITING, LabelString, 40);
		Static_SetText(MainWnd::LabelTime, LabelString);
		LoadString(NULL, BUTTON_CANCEL, LabelString, 40);
		Button_SetText(MainWnd::ButtonSub, LabelString);
		LoadString(NULL, BUTTON_STOP, LabelString, 40);
		Button_SetText(MainWnd::ButtonMain, LabelString);
	}

	VOID Abort() {
		State = IDLE;
		TCHAR LabelString[40];
		if (Last) {
			LoadString(NULL, LABEL_TOTAL_TIME, LabelString, 40);
			Static_SetText(MainWnd::LabelTime, LabelString);
			LoadString(NULL, BUTTON_SAVE, LabelString, 40);
			Button_SetText(MainWnd::ButtonSub, LabelString);
		}
		else {
			LoadString(NULL, LABEL_NO_VIDEO, LabelString, 40);
			Static_SetText(MainWnd::LabelTime, LabelString);
			LoadString(NULL, BUTTON_OPEN, LabelString, 40);
			Button_SetText(MainWnd::ButtonSub, LabelString);
		}
		LoadString(NULL, BUTTON_START, LabelString, 40);
		Button_SetText(MainWnd::ButtonMain, LabelString);
	}

	VOID WaitClose() {
		if (Last) {
			UnloadClose();
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
		}
	}
	VOID WaitDelete() {
		if (Last) {
			UnloadSession(GetSession());
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
		}
	}

	VOID Record() {
		if (Last && 1000 > INFINITE - Last->Time || !Parser->FixEnterGame(Proxy::Server)) {
			return Proxy::Server.Discard();
		}
		if (!(Current = Last ? Last->Next = new(std::nothrow) Session(Last) : First = new(std::nothrow) Session())) {
			return Proxy::Server.Discard();
		}
		Timer::Start();
		Current->Record(Proxy::Server);
		State = RECORD;
		TimeStr::SetTime(Current->Time);
		ListBox_SetCurSel(MainWnd::ListSessions, ListBox_AddString(MainWnd::ListSessions, Parser->Character->Name.Data));
		ListBox_Enable(MainWnd::ListSessions, FALSE);
		TCHAR LabelString[40];
		LoadString(NULL, LABEL_RECORDING, LabelString, 40);
		Static_SetText(MainWnd::LabelTime, LabelString);
		Static_SetText(MainWnd::StatusTime, TimeStr::Time);
		MainWnd::Focus(MainWnd::ButtonMain);
	}
	VOID RecordNext() {
		if (Parser->EnterGame) {
			if (1000 > INFINITE - Current->Time || !Parser->FixEnterGame(Proxy::Server)) {
				Proxy::Server.Discard();
				return Continue();
			}
			if (!(Current->Next = new(std::nothrow) Session(Current))) {
				Proxy::Server.Discard();
				return Continue();
			}
			Timer::Restart();
			Current->EndSession();
			Last = Current;
			Changed = TRUE;
			Current = Current->Next;
			Current->Record(Proxy::Server);
			LastTimeChanged();
			CONST INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
			ListBox_SetItemData(MainWnd::ListSessions, ListBox_InsertString(MainWnd::ListSessions, LoginNumber, TimeStr::Set(LoginNumber, Last->Login->SessionTime(), Last->Time)), Last->Login);
		}
		else {
			if (Parser->Pending || Parser->PlayerData || !Parser->FixTrade(Proxy::Server)) {
				Proxy::Server.Discard();
				return Continue();
			}
			DWORD Delay = Timer::Elapsed();
			if (Delay > BIGDELAY) {
				Delay = BIGDELAY;
			}
			if (Delay > INFINITE - Current->Time) {
				Delay = INFINITE - Current->Time;
			}
			if (!(Current->Next = new(std::nothrow) Packet(Current, WORD(Delay)))) {
				Proxy::Server.Discard();
				return Continue();
			}
			Current = Current->Next;
			Current->Record(Proxy::Server);
		}
		TimeStr::SetTime(Current->Time);
		Static_SetText(MainWnd::StatusTime, TimeStr::Time);
	}
	
	VOID Cancel() {
		Timer::Stop();
		State = IDLE;
		Current = Current->Login;
		TCHAR LabelString[40];
		if (Last) {
			Last->Next = NULL;
			Unload();
			TimeStr::SetTime(Last->Time);
			SetWindowRedraw(MainWnd::ListSessions, FALSE);
			ListBox_DeleteString(MainWnd::ListSessions, ListBox_GetCurSel(MainWnd::ListSessions));
			ListBox_Enable(MainWnd::ListSessions, TRUE);
			SetWindowRedraw(MainWnd::ListSessions, TRUE);
			ListBox_SetCurSel(MainWnd::ListSessions, 0);
			LoadString(NULL, LABEL_TOTAL_TIME, LabelString, 40);
			Static_SetText(MainWnd::LabelTime, LabelString);
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
			LoadString(NULL, BUTTON_SAVE, LabelString, 40);
			Button_SetText(MainWnd::ButtonSub, LabelString);
		}
		else {
			First = NULL;
			Unload();
			TimeStr::SetTimeSeconds(0);
			SetWindowRedraw(MainWnd::ListSessions, FALSE);
			ListBox_ResetContent(MainWnd::ListSessions);
			ListBox_Enable(MainWnd::ListSessions, TRUE);
			SetWindowRedraw(MainWnd::ListSessions, TRUE);
			LoadString(NULL, LABEL_NO_VIDEO, LabelString, 40);
			Static_SetText(MainWnd::LabelTime, LabelString);
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
			LoadString(NULL, BUTTON_OPEN, LabelString, 40);
			Button_SetText(MainWnd::ButtonSub, LabelString);
		}
		LoadString(NULL, BUTTON_START, LabelString, 40);
		Button_SetText(MainWnd::ButtonMain, LabelString);
	}

	VOID SaveSession() {
		Timer::Stop();
		Current->EndSession();
		Last = Current;
		Changed = TRUE;
		LastTimeChanged();
		SetWindowRedraw(MainWnd::ListSessions, FALSE);
		ListBox_DeleteString(MainWnd::ListSessions, ListBox_GetCurSel(MainWnd::ListSessions));
		ListBox_SetItemData(MainWnd::ListSessions, ListBox_AddString(MainWnd::ListSessions, TimeStr::Set(ListBox_GetCount(MainWnd::ListSessions), Current->Login->SessionTime(), Current->Time)), Current->Login);
		ListBox_Enable(MainWnd::ListSessions, TRUE);
		SetWindowRedraw(MainWnd::ListSessions, TRUE);
		ListBox_SetCurSel(MainWnd::ListSessions, 0);
	}

	VOID Continue() {
		State = WAIT;
		SaveSession();
		TCHAR LabelString[40];
		LoadString(NULL, LABEL_WAITING, LabelString, 40);
		Static_SetText(MainWnd::LabelTime, LabelString);
		MainWnd::Focus(MainWnd::ListSessions);
	}

	VOID Stop() {
		State = IDLE;
		SaveSession();
		TCHAR LabelString[40];
		LoadString(NULL, LABEL_TOTAL_TIME, LabelString, 40);
		Static_SetText(MainWnd::LabelTime, LabelString);
		LoadString(NULL, BUTTON_SAVE, LabelString, 40);
		Button_SetText(MainWnd::ButtonSub, LabelString);
		LoadString(NULL, BUTTON_START, LabelString, 40);
		Button_SetText(MainWnd::ButtonMain, LabelString);
	}

	VOID SetPlayed() {
		MainWnd::ScrollInfo.nTrackPos = PlayedTime / 1000;
		if (MainWnd::ScrollInfo.nPos != MainWnd::ScrollInfo.nTrackPos) {
			TimeStr::SetTimeSeconds(MainWnd::ScrollInfo.nPos = MainWnd::ScrollInfo.nTrackPos);
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
		}
		ScrollBar_SetInfo(MainWnd::ScrollPlayed, &MainWnd::ScrollInfo, TRUE);
	}

	BOOL SyncOne() {
		Parser->ConstructVideo();
		return Proxy::Client.SendPacket(*Current);
	}
	BOOL SyncTime() {
		while (PlayedTime >= Current->Next->Time) {
			Current = Current->Next;
			if (!SyncOne()) {
				return FALSE;
			}
		}
		return TRUE;
	}
	BOOL SyncEnd() {
		while (Current->Next) {
			Current = Current->Next;
			if (!SyncOne()) {
				return FALSE;
			}
		}
		return TRUE;
	}
	BOOL Sync() {
		return PlayedTime < Last->Time ? SyncTime() : SyncEnd();
	}

	BOOL PlaySession() {
		PlayedTime = Current->Time;
		SetPlayed();
		return SyncOne() && Sync();
	}
	BOOL PlayStill() {
		SetPlayed();
		return Sync();
	}
	BOOL PlayStart() {
		if (!PlayedTime) {
			return PlayStill();
		}
		PlayedTime = 0;
		SetPlayed();
		if (Current->Time) {
			if (Current->Login->Time) {
				ListBox_SetCurSel(MainWnd::ListSessions, 0);
			}
			Current = First;
			if (!SyncOne()) {
				return FALSE;
			}
		}
		return SyncTime();
	}
	BOOL PlayBackward(CONST DWORD Elapsed) { //Elapsed must not be 0
		if (Elapsed >= PlayedTime) {
			return PlayStart();
		}
		PlayedTime -= Elapsed;
		SetPlayed();
		if (PlayedTime < Current->Time) {
			Current = Current->Login;
			if (PlayedTime < Current->Time) {
				INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
				do {
					Current = CurrentLogin->Prev->Login;
					LoginNumber--;
				} while (PlayedTime < Current->Time);
				ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
			}
			if (!SyncOne()) {
				return FALSE;
			}
		}
		return SyncTime();
	}
	BOOL PlayEnd() {
		PlayedTime = Last->Time;
		SetPlayed();
		if (Current->Login->Last->Next) {
			Current = Last->Login;
			ListBox_SetCurSel(MainWnd::ListSessions, ListBox_GetCount(MainWnd::ListSessions) - 1);
			if (!SyncOne()) {
				return FALSE;
			}
		}
		return SyncEnd();
	}
	BOOL PlayForward(CONST DWORD Elapsed) {
		if (Elapsed >= Last->Time - PlayedTime) {
			return PlayEnd();
		}
		PlayedTime += Elapsed;
		SetPlayed();
		if (Current->Login->Last->Next && PlayedTime >= Current->Login->Last->Next->Time) {
			Current = Current->Login;
			INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
			do {
				Current = CurrentLogin->Last->Next;
				LoginNumber++;
			} while (CurrentLogin->Last->Next && PlayedTime >= CurrentLogin->Last->Next->Time);
			ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
			if (!SyncOne()) {
				return FALSE;
			}
		}
		return SyncTime();
	}
	BOOL PlayPosition(CONST DWORD Time) {
		return Time < PlayedTime ? PlayBackward(PlayedTime - Time) : PlayForward(Time - PlayedTime);
	}

	BOOL PlayElapsed() {
		return PlayForward(CalcElapsed(Timer::Elapsed()));
	}
	BOOL PlayResuming() {
		return Current->Next ? PlayStill() : PlayStart();
	}
	
	BOOL PlayElapsedBackward(CONST DWORD Time, CONST DWORD Elapsed) {
		return Elapsed < Time ? PlayBackward(Time - Elapsed) : PlayForward(Elapsed - Time);
	}
	BOOL PlayElapsedForward(CONST DWORD Time, CONST DWORD Elapsed) {
		return Elapsed < INFINITE - Time ? PlayForward(Time + Elapsed) : PlayEnd();
	}

	VOID SetSpeedLabel(CONST LPTSTR LabelString) {
		if (Speed > 0) {
			CopyMemory(LabelString + LoadString(NULL, LABEL_FAST, LabelString, 34), SpeedLabel[Speed - 1], TLEN(6));
		}
		else if (Speed < 0) {
			CopyMemory(LabelString + LoadString(NULL, LABEL_SLOW, LabelString, 34), SpeedLabel[(-Speed) - 1], TLEN(6));
		}
		else {
			LoadString(NULL, LABEL_PLAYING, LabelString, 40);
		}
	}
	VOID TimerPlaying() {
		DWORD Waste = 1000 - PlayedTime % 1000;
		DWORD Delay = Current->Next->Time - PlayedTime;
		Delay = CalcDelay(min(Waste, Delay));
		Waste = Timer::Partial();
		SetTimer(MainWnd::Handle, IDTIMER, Delay > Waste ? Delay - Waste : 0, NULL);
		Tibia::CloseDialogs();
	}
	VOID TimerSetPlaying() {
		TCHAR LabelString[40];
		SetSpeedLabel(LabelString);
		Static_SetText(MainWnd::LabelTime, LabelString);
		LoadString(NULL, BUTTON_PAUSE, LabelString, 40);
		Button_SetText(MainWnd::ButtonMain, LabelString);
		SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
		Timer::Restart();
		TimerPlaying();
	}
	VOID TimerPaused() {
		SetTimer(MainWnd::Handle, IDTIMER, IDLETIME, NULL);
		Tibia::CloseDialogs();
	}
	VOID TimerSetPaused(BOOL Finished) {
		TCHAR LabelString[40];
		LoadString(NULL, LABEL_PAUSED, LabelString, 40);
		Static_SetText(MainWnd::LabelTime, LabelString);
		LoadString(NULL, Finished ? BUTTON_RESTART : BUTTON_PLAY, LabelString, 40);
		Button_SetText(MainWnd::ButtonMain, LabelString);
		SetThreadExecutionState(ES_CONTINUOUS);
		Speed = PAUSED;
		TimerPaused();
	}

	BOOL SendPing() {
		return Proxy::Client.SendPacket(Proxy::Server);
	}
	BOOL SendFinished() {
		return Proxy::SendClientMessage(ID_GAME_INFO, MESSAGE_END_VIDEO);
	}

	BOOL PlayerFinish() {
		TimerSetPaused(TRUE);
		return SendFinished();
	}
	BOOL PlayerContinue() {
		if (!Current->Next) {
			return PlayerFinish();
		}
		TimerPlaying();
		return SendPing();
	}
	BOOL PlayerSpeedChange(CONST INT NewSpeed) {
		if (!Current->Next) {
			return PlayerFinish();
		}
		Speed = NewSpeed;
		TCHAR LabelString[40];
		SetSpeedLabel(LabelString);
		Static_SetText(MainWnd::LabelTime, LabelString);
		TimerPlaying();
		return SendPing();
	}
	BOOL PlayerPause() {
		if (!Current->Next) {
			return PlayerFinish();
		}
		TimerSetPaused(FALSE);
		return SendPing();
	}
	BOOL PlayerFinished() {
		TimerPaused();
		return SendFinished();
	}
	BOOL PlayerResume(CONST INT NewSpeed) {
		if (!Current->Next) {
			return PlayerFinished();
		}
		Speed = NewSpeed;
		TimerSetPlaying();
		return SendPing();
	}
	BOOL PlayerPaused() {
		if (!Current->Next) {
			TCHAR LabelString[40];
			LoadString(NULL, BUTTON_RESTART, LabelString, 40);
			Button_SetText(MainWnd::ButtonMain, LabelString);
			return PlayerFinished();
		}
		TimerPaused();
		return SendPing();
	}
	
	VOID Play() {
		Parser->ConstructVideoPing();
		if (!Proxy::Server) {
			return;
		}
		State = PLAY;
		MainWnd::ScrollInfo.nPos = MainWnd::ScrollInfo.nMax;
		ListBox_SetCurSel(MainWnd::ListSessions, GetSession());
		ScrollBar_Enable(MainWnd::ScrollPlayed, TRUE); //Not needed, called for WINE compatibility
		if (!PlaySession()) {
			return Logout();
		}
		if (!Current->Next) {
			TimerSetPaused(TRUE);
			if (!SendFinished()) {
				return Logout();
			}
		}
		else {
			TimerSetPlaying();
			if (!SendPing()) {
				return Logout();
			}
		}
		Button_Enable(MainWnd::ButtonSpeedUp, TRUE);
		Button_Enable(MainWnd::ButtonSlowDown, TRUE);
		TCHAR LabelString[40];
		LoadString(NULL, BUTTON_STOP, LabelString, 40);
		Button_SetText(MainWnd::ButtonSub, LabelString);
		MainWnd::Focus(MainWnd::ScrollPlayed);
	}

	VOID PlayerStop() {
		Tibia::Unlock();
		Proxy::Server.Discard(); // ping packet
		Proxy::HandleClientClose();
		Speed = 0;
		KillTimer(MainWnd::Handle, IDTIMER);
		SetThreadExecutionState(ES_CONTINUOUS);
		Button_Enable(MainWnd::ButtonSlowDown, FALSE);
		Button_Enable(MainWnd::ButtonSpeedUp, FALSE);
		ScrollBar_Enable(MainWnd::ScrollPlayed, FALSE);
	}
	VOID Eject() {
		Parser->RewindVideo();
		TimeStr::SetTime(Last->Time);
		State = IDLE;
		PlayerStop();
		TCHAR LabelString[40];
		LoadString(NULL, LABEL_TOTAL_TIME, LabelString, 40);
		Static_SetText(MainWnd::LabelTime, LabelString);
		Static_SetText(MainWnd::StatusTime, TimeStr::Time);
		LoadString(NULL, BUTTON_SAVE, LabelString, 40);
		Button_SetText(MainWnd::ButtonSub, LabelString);
		LoadString(NULL, BUTTON_START, LabelString, 40);
		Button_SetText(MainWnd::ButtonMain, LabelString);
	}

	VOID PlayerLogout() {
		State = WAIT;
		PlayerStop();
		TCHAR LabelString[40];
		LoadString(NULL, LABEL_WAITING, LabelString, 40);
		Static_SetText(MainWnd::LabelTime, LabelString);
		Static_SetText(MainWnd::StatusTime, TimeStr::Time);
		LoadString(NULL, BUTTON_CANCEL, LabelString, 40);
		Button_SetText(MainWnd::ButtonSub, LabelString);
		LoadString(NULL, BUTTON_STOP, LabelString, 40);
		Button_SetText(MainWnd::ButtonMain, LabelString);
		MainWnd::Focus(MainWnd::ListSessions);
	}
	VOID Logout() {
		Parser->RewindVideo();
		TimeStr::SetTime(Last->Time);
		PlayerLogout();
	}
	VOID PlayClose() {
		UnloadClose();
		PlayerLogout();
	}

	BOOL SinglePacket() {
		if (Current->Next) {
			if (Current->IsLast()) {
				ListBox_SetCurSel(MainWnd::ListSessions, ListBox_GetCurSel(MainWnd::ListSessions) + 1);
			}
			Current = Current->Next;
			PlayedTime = Current->Time;
			SetPlayed();
		}
		else {
			Current = First;
			PlayedTime = 0;
			SetPlayed();
			if (ListBox_GetCurSel(MainWnd::ListSessions)) {
				ListBox_SetCurSel(MainWnd::ListSessions, 0);
			}
		}
		TimerPaused();
		return SyncOne();
	}

	VOID PlayTimer() {
		if (Speed > PAUSED) {
			if (!PlayElapsed()) {
				return Logout();
			}
			if (!PlayerContinue()) {
				return Logout();
			}
		}
		else {
			if (Current->Next) {
				Tibia::CloseDialogs();
			}
			if (!SendPing()) {
				return Logout();
			}
		}
	}

	VOID PlayPause() {
		if (Speed > PAUSED) {
			if (!PlayElapsed()) {
				return Logout();
			}
			if (!PlayerPause()) {
				return Logout();
			}
		}
		else {
			if (!PlayResuming()) {
				return Logout();
			}
			if (!PlayerResume(0)) {
				return Logout();
			}
		}
	}
	VOID Resume() {
		if (Speed > PAUSED) {
			if (!PlayElapsed()) {
				return Logout();
			}
			if (!(Speed ? PlayerSpeedChange(0) : PlayerContinue())) {
				return Logout();
			}
		}
		else {
			if (!PlayResuming()) {
				return Logout();
			}
			if (!PlayerResume(0)) {
				return Logout();
			}
		}
	}
	VOID Pause() {
		if (Speed > PAUSED) {
			if (!PlayElapsed()) {
				return Logout();
			}
			if (!PlayerPause()) {
				return Logout();
			}
		}
		else {
			if (!SinglePacket()) {
				return Logout();
			}
		}
	}

	VOID SlowDown() {
		if (Speed > PAUSED) {
			if (!PlayElapsed()) {
				return Logout();
			}
			if (Speed > PAUSED+1) {
				if (!PlayerSpeedChange(Speed - 1)) {
					return Logout();
				}
			}
			else {
				if (!PlayerPause()) {
					return Logout();
				}
			}
		}
		else {
			if (!SinglePacket()) {
				return Logout();
			}
		}
	}
	VOID SpeedUp() {
		if (Speed > PAUSED) {
			if (Speed < LIGHTSPEED-1) {
				if (!PlayElapsed()) {
					return Logout();
				}
				if (!PlayerSpeedChange(Speed + 1)) {
					return Logout();
				}
			}
			else {
				if (!PlayEnd()) {
					return Logout();
				}
				if (!PlayerFinish()) {
					return Logout();
				}
			}
		}
		else {
			if (!PlayResuming()) {
				return Logout();
			}
			if (!PlayerResume(PAUSED+1)) {
				return Logout();
			}
		}
	}
	VOID SetSpeed(CONST INT NewSpeed) {
		if (Speed) {
			if (NewSpeed < LIGHTSPEED) {
				if (!PlayElapsed()) {
					return Logout();
				}
				if (NewSpeed > PAUSED) {
					if (!(NewSpeed != Speed ? PlayerSpeedChange(NewSpeed) : PlayerContinue())) {
						return Logout();
					}
				}
				else {
					if (!PlayerPause()) {
						return Logout();
					}
				}
			}
			else {
				if (!PlayEnd()) {
					return Logout();
				}
				if (!PlayerFinish()) {
					return Logout();
				}
			}
		}
		else {
			if (NewSpeed < LIGHTSPEED) {
				if (NewSpeed > PAUSED) {
					if (!PlayResuming()) {
						return Logout();
					}
					if (!PlayerResume(NewSpeed)) {
						return Logout();
					}
				}
				else {
					if (!SinglePacket()) {
						return Logout();
					}
				}
			}
			else {
				if (!PlayEnd()) {
					return Logout();
				}
				if (!PlayerFinished()) {
					return Logout();
				}
			}
		}
	}
	VOID SetLight(CONST BYTE Light) {
		Parser->ConstructPlayerLight(Current->Login->PlayerID, Light);
		if (!Proxy::SendConstructed()) {
			return Logout();
		}
	}

	DWORD StartSkip() {
		TCHAR LabelString[40];
		if (Speed > PAUSED) {
			CONST DWORD Elapsed = CalcElapsed(Timer::Elapsed());
			if (Elapsed >= Last->Time - PlayedTime) {
				Speed = PAUSED;
				SetThreadExecutionState(ES_CONTINUOUS);
				LoadString(NULL, LABEL_PAUSED, LabelString, 40);
				Static_SetText(MainWnd::LabelTime, LabelString);
				LoadString(NULL, BUTTON_PLAY, LabelString, 40);
				Button_SetText(MainWnd::ButtonMain, LabelString);
				return Last->Time - PlayedTime;
			}
			return Elapsed;
		}
		Timer::Restart();
		LoadString(NULL, BUTTON_PLAY, LabelString, 40);
		Button_SetText(MainWnd::ButtonMain, LabelString);
		return 0;
	}
	BOOL EndSkip() {
		return Speed > PAUSED ? PlayerContinue() : PlayerPaused();
	}
	
	INT StartSkipSession() {
		PlayedTime += StartSkip();
		Current = Current->Login;
		INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
		while (CurrentLogin->Last->Next && PlayedTime >= CurrentLogin->Last->Next->Time) {
			Current = CurrentLogin->Last->Next;
			LoginNumber++;
		}
		return LoginNumber;
	}

	VOID PlayDelete() {
		if (!(Current = UnloadSession(StartSkipSession()))) {
			return PlayerLogout();
		}
		if (!PlaySession()) {
			return Logout();
		}
		if (!EndSkip()) {
			return Logout();
		}
	}

	VOID SessionSelect() {
		StartSkip();
		GetSession();
		if (!PlaySession()) {
			return Logout();
		}
		if (!EndSkip()) {
			return Logout();
		}
	}
	VOID SessionSelect(INT NewNumber) {
		StartSkip();
		CONST INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
		if (ListBox_SetCurSel(MainWnd::ListSessions, NewNumber) < 0) {
			NewNumber = ListBox_GetCount(MainWnd::ListSessions);
			if (NewNumber > 0) {
				NewNumber--;
			}
			else {
				NewNumber = LoginNumber;
			}
			ListBox_SetCurSel(MainWnd::ListSessions, NewNumber);
		}
		if (NewNumber == LoginNumber) {
			Current = Current->Login;
		}
		else {
			Current = (Packet *) ListBox_GetItemData(MainWnd::ListSessions, NewNumber);
		}
		if (!PlaySession()) {
			return Logout();
		}
		if (!EndSkip()) {
			return Logout();
		}
	}
	VOID SessionStart() {
		ListBox_SetCurSel(MainWnd::ListSessions, StartSkipSession());
		if (!PlaySession()) {
			return Logout();
		}
		if (!EndSkip()) {
			return Logout();
		}
	}
	VOID SessionNext() {
		INT LoginNumber = StartSkipSession();
		if (CurrentLogin->Last->Next) {
			Current = CurrentLogin->Last->Next;
			LoginNumber++;
		}
		ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
		if (!PlaySession()) {
			return Logout();
		}
		if (!EndSkip()) {
			return Logout();
		}
	}
	VOID SessionPrev() {
		INT LoginNumber = StartSkipSession();
		if (CurrentLogin->Prev) {
			Current = CurrentLogin->Prev->Login;
			LoginNumber--;
		}
		ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
		if (!PlaySession()) {
			return Logout();
		}
		if (!EndSkip()) {
			return Logout();
		}
	}

	VOID SkipStart() {
		StartSkip();
		if (!PlayStart()) {
			return Logout();
		}
		if (!EndSkip()) {
			return Logout();
		}
	}
	VOID SkipBackward(CONST DWORD Time) {
		if (!PlayElapsedBackward(Time, StartSkip())) {
			return Logout();
		}
		if (!EndSkip()) {
			return Logout();
		}
	}
	VOID SkipEnd() {
		StartSkip();
		if (!PlayEnd()) {
			return Logout();
		}
		if (!EndSkip()) {
			return Logout();
		}
	}
	VOID SkipForward(CONST DWORD Time) {
		if (!PlayElapsedForward(Time, StartSkip())) {
			return Logout();
		}
		if (!EndSkip()) {
			return Logout();
		}
	}
	VOID SkipPosition(CONST DWORD Time) {
		StartSkip();
		if (!PlayPosition(Time)) {
			return Logout();
		}
		if (!EndSkip()) {
			return Logout();
		}
	}

	BOOL ScrollBreakLoop() {
		return !MsgWaitForMultipleObjects(0, NULL, FALSE, 0, QS_TIMER | QS_KEY | QS_MOUSEBUTTON);
	}
	BOOL ScrollSyncOne() {
		Tibia::Lock();
		return SyncOne();
	}
	BOOL ScrollSyncTime() {
		while (PlayedTime >= Current->Next->Time) {
			Current = Current->Next;
			if (!ScrollSyncOne()) {
				return FALSE;
			}
			if (ScrollBreakLoop()) {
				return TRUE;
			}
		}
		Tibia::Redraw();
		return SendPing();
	}
	BOOL ScrollSyncEnd() {
		while (Current->Next) {
			Current = Current->Next;
			if (!ScrollSyncOne()) {
				return FALSE;
			}
			if (ScrollBreakLoop()) {
				return TRUE;
			}
		}
		Tibia::Redraw();
		return SendPing();
	}
	BOOL ScrollSync() {
		return PlayedTime < Last->Time ? ScrollSyncTime() : ScrollSyncEnd();
	}

	BOOL ScrollStill() {
		SetPlayed();
		return ScrollSync();
	}
	BOOL ScrollStart() {
		if (!PlayedTime) {
			return ScrollStill();
		}
		PlayedTime = 0;
		SetPlayed();
		if (Current->Time) {
			if (Current->Login->Time) {
				ListBox_SetCurSel(MainWnd::ListSessions, 0);
			}
			Current = First;
			Tibia::Redraw();
			if (!ScrollSyncOne()) {
				return FALSE;
			}
			if (ScrollBreakLoop()) {
				return TRUE;
			}
		}
		return ScrollSyncTime();
	}
	BOOL ScrollBackward(CONST DWORD Elapsed) { //Elapsed must not be 0
		if (Elapsed >= PlayedTime) {
			return ScrollStart();
		}
		PlayedTime -= Elapsed;
		SetPlayed();
		if (PlayedTime < Current->Time) {
			Current = Current->Login;
			if (PlayedTime < Current->Time) {
				INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
				do {
					Current = CurrentLogin->Prev->Login;
					LoginNumber--;
				} while (PlayedTime < Current->Time);
				ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
			}
			Tibia::Redraw();
			if (!ScrollSyncOne()) {
				return FALSE;
			}
			if (ScrollBreakLoop()) {
				return TRUE;
			}
		}
		return ScrollSyncTime();
	}
	BOOL ScrollEnd() {
		PlayedTime = Last->Time;
		SetPlayed();
		if (Current->Login->Last->Next) {
			Current = Last->Login;
			ListBox_SetCurSel(MainWnd::ListSessions, ListBox_GetCount(MainWnd::ListSessions) - 1);
			if (!ScrollSyncOne()) {
				return FALSE;
			}
			if (ScrollBreakLoop()) {
				return TRUE;
			}
		}
		return ScrollSyncEnd();
	}
	BOOL ScrollForward(CONST DWORD Elapsed) {
		if (Elapsed >= Last->Time - PlayedTime) {
			return ScrollEnd();
		}
		PlayedTime += Elapsed;
		SetPlayed();
		if (Current->Login->Last->Next && PlayedTime >= Current->Login->Last->Next->Time) {
			Current = Current->Login;
			INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
			do {
				Current = CurrentLogin->Last->Next;
				LoginNumber++;
			} while (CurrentLogin->Last->Next && PlayedTime >= CurrentLogin->Last->Next->Time);
			ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
			if (!ScrollSyncOne()) {
				return FALSE;
			}
			if (ScrollBreakLoop()) {
				return TRUE;
			}
		}
		return ScrollSyncTime();
	}
	BOOL ScrollPosition(CONST DWORD Time) {
		return Time < PlayedTime ? ScrollBackward(PlayedTime - Time) : ScrollForward(Time - PlayedTime);
	}

	BOOL ScrollElapsedBackward(CONST DWORD Time, CONST DWORD Elapsed) {
		return Elapsed < Time ? ScrollBackward(Time - Elapsed) : ScrollForward(Elapsed - Time);
	}
	BOOL ScrollElapsedForward(CONST DWORD Time, CONST DWORD Elapsed) {
		return Elapsed < INFINITE - Time ? ScrollForward(Time + Elapsed) : ScrollEnd();
	}

	VOID SkipScroll(CONST WORD Code) {
		UpdateWindow(MainWnd::Handle);
		CONST DWORD Elapsed = StartSkip();
		SetTimer(MainWnd::Handle, IDTIMER, 0, NULL);
		switch (Code) {
			case SB_LINELEFT:
				if (!ScrollElapsedBackward(1000, Elapsed)) {
					return Logout();
				}
				break;
			case SB_LINERIGHT:
				if (!ScrollElapsedForward(1000, Elapsed)) {
					return Logout();
				}
				break;
			case SB_PAGELEFT:
				if (!ScrollElapsedBackward(60000, Elapsed)) {
					return Logout();
				}
				break;
			case SB_PAGERIGHT:
				if (!ScrollElapsedForward(60000, Elapsed)) {
					return Logout();
				}
				break;
			case SB_THUMBPOSITION:
				break;
			case SB_THUMBTRACK:
				if (!ScrollStill()) {
					return Logout();
				}
				break;
			case SB_LEFT:
				if (!ScrollStart()) {
					return Logout();
				}
				break;
			case SB_RIGHT:
				if (!ScrollEnd()) {
					return Logout();
				}
				break;
			default:
				if (!PlayForward(Elapsed)) {
					return Logout();
				}
				if (!EndSkip()) {
					return Logout();
				}
				return;
		}
		State = SCROLL;
	}
	VOID ScrollRepeat(CONST WORD Code) {
		CONST DWORD Elapsed = Timer::Elapsed();
		SetTimer(MainWnd::Handle, IDTIMER, 0, NULL);
		switch (Code) {
			case SB_LINELEFT:
				if (!ScrollBackward(Elapsed <= 50 ? 1000 : Elapsed < 100 ? Elapsed * 20 : 2000)) {
					return Logout();
				}
				break;
			case SB_LINERIGHT:
				if (!ScrollForward(Elapsed <= 50 ? 1000 : Elapsed < 100 ? Elapsed * 20 : 2000)) {
					return Logout();
				}
				break;
			case SB_PAGELEFT:
				if (!ScrollBackward(Elapsed <= 50 ? 60000 : Elapsed < 100 ? Elapsed * 1200 : 120000)) {
					return Logout();
				}
				break;
			case SB_PAGERIGHT:
				if (!ScrollForward(Elapsed <= 50 ? 60000 : Elapsed < 100 ? Elapsed * 1200 : 120000)) {
					return Logout();
				}
				break;
			case SB_THUMBPOSITION:
				if (!ScrollStill()) {
					return Logout();
				}
				break;
			case SB_THUMBTRACK:
				ScrollBar_GetInfo(MainWnd::ScrollPlayed, &MainWnd::ScrollInfo);
				if (!ScrollPosition(DWORD(MainWnd::ScrollInfo.nTrackPos) * 1000)) {
					return Logout();
				}
				break;
			case SB_LEFT:
				if (!ScrollStart()) {
					return Logout();
				}
				break;
			case SB_RIGHT:
				if (!ScrollEnd()) {
					return Logout();
				}
				break;
			case SB_ENDSCROLL:
				SetCursor(LoadCursor(NULL, IDC_APPSTARTING));
				UpdateWindow(MainWnd::Handle);
				MainWnd::Progress_Start();
				if (PlayedTime < Last->Time) {
					while (PlayedTime >= Current->Next->Time) {
						Current = Current->Next;
						if (!ScrollSyncOne()) {
							SetCursor(LoadCursor(NULL, IDC_ARROW));
							return Logout();
						}
						MainWnd::Progress_Set(Current->TimeInSession(), Current->Login->SessionTime());
					}
				}
				else {
					while (Current->Next) {
						Current = Current->Next;
						if (!ScrollSyncOne()) {
							SetCursor(LoadCursor(NULL, IDC_ARROW));
							return Logout();
						}
						MainWnd::Progress_Set(Current->TimeInSession(), Current->Login->SessionTime());
					}
				}
				Tibia::Redraw();
				SetCursor(LoadCursor(NULL, IDC_ARROW));
				Timer::Restart();
				MainWnd::Progress_Stop();
				if (!EndSkip()) {
					return Logout();
				}
				State = PLAY;
				break;
		}
	}
	VOID ScrollIdle() {
		if (!ScrollStill()) {
			return Logout();
		}
	}

	VOID ScrollSession() {
		PlayedTime = Current->Time;
		SetPlayed();
		if (!ScrollSyncOne()) {
			return Logout();
		}
	}
	VOID ScrollSessionSelect() {
		GetSession();
		ScrollSession();
	}
	VOID ScrollDelete() {
		if (!(Current = UnloadSession(GetSession()))) {
			return PlayerLogout();
		}
		ScrollSession();
	}

	VOID ForcePause() {
		SetPlayed();
		if (Speed > PAUSED) {
			TimerSetPaused(PlayedTime >= Last->Time);
		}
		else {
			if (PlayedTime >= Last->Time) {
				TCHAR LabelString[40];
				LoadString(NULL, BUTTON_RESTART, LabelString, 40);
				Button_SetText(MainWnd::ButtonMain, LabelString);
			}
			TimerPaused();
		}
	}

	VOID PlayFirstPacket() {
		PlayedTime = Current->Login->Time;
		ForcePause();
		Current = Current->Login;
		if (!SyncOne()) {
			return Logout();
		}
	}
	VOID PlayLastPacket() {
		PlayedTime = Current->Login->Last->Time;
		ForcePause();
		if (Current->IsLast()) {
			if (!SendPing()) {
				return Logout();
			}
		}
		else {
			if (!Sync()) {
				return Logout();
			}
		}
	}
	
	VOID CutStart() {
		if (PlayedTime > Current->Login->Last->Time) {
			PlayedTime = Current->Login->Last->Time;
		}
		CONST INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
		if (PlayedTime <= Current->Login->Time) {
			ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
		}
		else {
			Packet* CONST Backup = Current;
			Current = Current->Login->Next;
			while (PlayedTime > Current->Time) {
				Current->Time = Current->Login->Time;
				Current = Current->Next;
			}
			PlayedTime -= Current->Login->Time;
			do {
				Current->Time -= PlayedTime;
			} while (Current = Current->Next);
			Changed = TRUE;
			PlayedTime = Backup->Time;
			SessionTimeChanged(LoginNumber);
			Current = Backup;
		}
		ForcePause();
		if (!Sync()) {
			return Logout();
		}
		if (!Proxy::SendClientMessage(ID_GAME_INFO, MESSAGE_EDIT_START)) {
			return Logout();
		}
	}
	VOID CutEnd() {
		CONST INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
		if (Current->IsLast()) {
			ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
		}
		else {
			Packet *CONST Backup = Current;
			Current = Current->Next;
			if (Backup->Next = Backup->Login->Last->Next) {
				Backup->CutSession();
				Backup->Login->Last->Next = NULL;
				PlayedTime = Backup->Login->Last->Time - Backup->Time;
				Unload();
				if (PlayedTime) {
					Current = Backup->Next;
					do {
						Current->Time -= PlayedTime;
					} while (Current = Current->Next);
				}
			}
			else {
				Last = Backup;
				Unload();
			}
			Backup->EndSession();
			Changed = TRUE;
			PlayedTime = Backup->Time;
			SessionTimeChanged(LoginNumber);
			Current = Backup;
		}
		ForcePause();
		if (!Proxy::SendClientMessage(ID_GAME_INFO, MESSAGE_EDIT_END)) {
			return Logout();
		}
	}
	VOID AddFast(DWORD TimeBias) {
		CONST INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
		DWORD Target = Current->Login->Last->Time;
		if (!TimeBias || PlayedTime >= Target) {
			ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
		}
		else {
			Packet* CONST Backup = Current;
			if (TimeBias < Target - PlayedTime) {
				Target = PlayedTime + TimeBias;
			}
			TimeBias = 0;
			DWORD LastTime = PlayedTime;
			while (Current->Next->Time < Target) {
				Current = Current->Next;
				TimeBias += (Current->Time - LastTime + 1) / 2;
				LastTime = Current->Time;
				Current->Time -= TimeBias;
			}
			Current = Current->Next;
			TimeBias += (Target - LastTime + 1) / 2;
			do {
				Current->Time -= TimeBias;
			} while (Current = Current->Next);
			Changed = TRUE;
			SessionTimeChanged(LoginNumber);
			Current = Backup;
		}
		ForcePause();
		if (!Proxy::SendClientMessage(ID_GAME_INFO, MESSAGE_EDIT_FAST)) {
			return Logout();
		}
	}
	VOID AddSlow(DWORD TimeBias) {
		CONST INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
		DWORD Target = Current->Login->Last->Time;
		if (!TimeBias || PlayedTime >= Target) {
			ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
		}
		else {
			Packet* CONST Backup = Current;
			if (TimeBias < Target - PlayedTime) {
				Target = PlayedTime + TimeBias;
			}
			TimeBias = 0;
			DWORD LastTime = PlayedTime;
			while (Current->Next->Time < Target) {
				if ((LastTime = Current->Next->Time - LastTime + 1) > BIGDELAY - (Current->Next->Time - Current->Time)) {
					LastTime = BIGDELAY - (Current->Next->Time - Current->Time);
				}
				Current = Current->Next;
				TimeBias += LastTime;
				LastTime = Current->Time;
				if (TimeBias > INFINITE - Last->Time) {
					TimeBias = INFINITE - Last->Time;
					Target = LastTime;
				}
				Current->Time += TimeBias;
			}
			if ((LastTime = Target - LastTime) > BIGDELAY - (Current->Next->Time - Current->Time)) {
				LastTime = BIGDELAY - (Current->Next->Time - Current->Time);
			}
			Current = Current->Next;
			TimeBias += LastTime;
			if (TimeBias > INFINITE - Last->Time) {
				TimeBias = INFINITE - Last->Time;
			}
			do {
				Current->Time += TimeBias;
			} while (Current = Current->Next);
			Changed = TRUE;
			SessionTimeChanged(LoginNumber);
			Current = Backup;
		}
		ForcePause();
		if (!Proxy::SendClientMessage(ID_GAME_INFO, MESSAGE_EDIT_SLOW)) {
			return Logout();
		}
	}
	VOID AddSkip(DWORD TimeBias) {
		CONST INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
		DWORD Target = Current->Login->Last->Time;
		if (!TimeBias || PlayedTime >= Target) {
			ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
		}
		else {
			Packet* CONST Backup = Current;
			if (TimeBias > Target - PlayedTime) {
				TimeBias = Target - PlayedTime;
			}
			else {
				Target = PlayedTime + TimeBias;
			}
			while (Current->Next->Time < Target) {
				Current = Current->Next;
				Current->Time = PlayedTime;
			}
			Current = Current->Next;
			do {
				Current->Time -= TimeBias;
			} while (Current = Current->Next);
			Changed = TRUE;
			SessionTimeChanged(LoginNumber);
			Current = Backup;
		}
		ForcePause();
		if (!Sync()) {
			return Logout();
		}
		if (!Proxy::SendClientMessage(ID_GAME_INFO, MESSAGE_EDIT_SKIP)) {
			return Logout();
		}
	}
	VOID AddDelay(WORD TimeBias) {
		CONST INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
		if (Current->IsLast()) {
			ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
		}
		else {
			if (TimeBias > BIGDELAY - (Current->Next->Time - Current->Time)) {
				TimeBias = WORD(BIGDELAY - (Current->Next->Time - Current->Time));
			}
			if (TimeBias > INFINITE - Last->Time) {
				TimeBias = WORD(INFINITE - Last->Time);
			}
			if (!TimeBias) {
				ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
			}
			else {
				Packet *CONST Backup = Current;
				Current = Current->Next;
				do {
					Current->Time += TimeBias;
				} while (Current = Current->Next);
				Changed = TRUE;
				PlayedTime += TimeBias;
				SessionTimeChanged(LoginNumber);
				Current = Backup;
			}
		}
		ForcePause();
		if (!Proxy::SendClientMessage(ID_GAME_INFO, MESSAGE_EDIT_DELAY)) {
			return Logout();
		}
	}
	VOID AddLight(CONST BYTE Light) {
		Parser->ConstructPlayerLight(Current->Login->PlayerID, Light);
		if (!Proxy::Extra) {
			return Logout();
		}
		Packet* Next = new(std::nothrow) Packet(Current, 0);
		if (!Next) {
			Proxy::Extra.Discard();
			return Logout();
		}
		Next->Record(Proxy::Extra);
		Next->Next = Current->Next;
		if (Current->IsLast()) {
			if (Next->Next) {
				Next->CutSession();
			}
			Next->EndSession();
		}
		Current = Current->Next = Next;
		Changed = TRUE;
		if (!Proxy::Client.SendPacket(*Current)) {
			return Logout();
		}
	}
}
