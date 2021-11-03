#include "video.h"
#include "main.h"
#include "file.h"
#include "tibia.h"
#include "proxy.h"
#include "loader.h"
#include "aes256.h"

#define CAM_HASH "TTM created CAM file - no hash."
#define BIGDELAY 0xFFFF
#define IDLETIME 10000
#define PAUSED (-4)
#define LIGHTSPEED 10
#define CalcDelay(Delay) (Speed > 0 ? ((Delay) >> Speed) + 1 : ((Delay) << -Speed))
#define CalcElapsed(Elapsed) (Speed < 0 ? ((Elapsed) >> -Speed) + 1 : (Elapsed) << Speed)

namespace Video {
	STATE State = IDLE;

	Session *Login = NULL;
	Packet *Current;
	Packet *Last = NULL;

	TCHAR FileName[MAX_PATH] = _T("");
	BYTE Changed = FALSE;

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

	DWORD PlayedTime;
	INT Speed = 0;
	CONST TCHAR SpeedLabel[][6] = { _T(" x2"), _T(" x4"), _T(" x8"), _T(" x16"), _T(" x32"), _T(" x64"), _T(" x128"), _T(" x256"), _T(" x512")};

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

	DWORD WINAPI ThreadUnload(Packet *Current) {
		Packet *Next;
		do {
			Next = Current->Next;
			delete Current;
			SwitchToThread();
		} while (Current = Next);
		return 0;
	}
	VOID Unload() {
		if (!CloseHandle(CreateThread(NULL, 1, LPTHREAD_START_ROUTINE(ThreadUnload), Current, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL))) {
			ThreadUnload(Current); //out of memory, let's free some from our own thread
		}
	}

	VOID SetFileTitle() {
		TCHAR Title[MAX_PATH];
		LPTSTR Name = PathFindFileName(FileName);
		SIZE_T NameLen = PathFindExtension(Name) - Name;
		if (NameLen > MAX_PATH - 22) {
			NameLen = MAX_PATH - 22;
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
		WritingFile File;
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
		PacketData *Packet = Parser->GetPacketData(*(Current = Login));
		if (!File.Write(Packet, Packet->RawSize())) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		MainWnd::Progress_Set(0, Last->Time);
		while (Current->Next) {
			if (Current->IsLast()) {
				if (!File.WriteByte(1)) {
					File.Delete(FileName);
					return ERROR_CANNOT_SAVE_VIDEO_FILE;
				}
			}
			else {
				if (!File.WriteByte(0)) {
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
			if (!Last) {
				if (!Tibia::Running) {
					Tibia::SetHost(Version, HostLen, Host, Port);
					if (!Loader().Run(Parent, TITLE_LOADER_OVERRIDE)) {
						if (Version < 700 || Version > LATEST) {
							Tibia::Version = Version < 700 ? 700 : LATEST;
							return ERROR_UNSUPPORTED_TIBIA_VERSION;
						}
						MainWnd::Progress_Start();
						Tibia::SetVersionString(Version);
						Override = FALSE;
					}
					else {
						Override = Tibia::Version != Version || Tibia::HostLen != HostLen || DiffMemory(Tibia::Host, Host, HostLen) || Tibia::Port != Port;
					}
				}
				else {
					Override = Tibia::Version != Version || Tibia::HostLen != HostLen || DiffMemory(Tibia::Host, Host, HostLen) || Tibia::Port != Port;
				}
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
					return ERROR_WRONG_VERSION;
				}
			}
		}
		return NULL;
	}

	VOID CancelOpen() {
		if (Last) {
			Current = Last->Next;
			Last->Next = NULL;
		}
		else {
			Current = Login;
			Login = NULL;
		}
		if (Current) {
			Unload();
		}
	}

	VOID FillSessionList() {
		Current = Login;
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
			Override = TRUE;
			INT NewSession = ListBox_GetCount(MainWnd::ListSessions);
			SetWindowRedraw(MainWnd::ListSessions, FALSE);
			while (Current = Last->Next) {
				ListBox_SetItemData(MainWnd::ListSessions, ListBox_AddString(MainWnd::ListSessions, TimeStr::Set(ListBox_GetCount(MainWnd::ListSessions), CurrentLogin->SessionTime(), CurrentLogin->Last->Time)), Current);
				Last = CurrentLogin->Last;
			}
			ListBox_SetCurSel(MainWnd::ListSessions, NewSession);
			SetWindowRedraw(MainWnd::ListSessions, TRUE);
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
		}
		else {
			Last = Current;
			Tibia::OpenVersionMenu();
			SetWindowRedraw(MainWnd::ListSessions, FALSE);
			FillSessionList();
			ListBox_SetCurSel(MainWnd::ListSessions, 0);
			SetWindowRedraw(MainWnd::ListSessions, TRUE);
			TCHAR LabelString[40];
			LoadString(NULL, LABEL_TOTAL_TIME, LabelString, 40);
			Static_SetText(MainWnd::LabelTime, LabelString);
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
			LoadString(NULL, BUTTON_SAVE, LabelString, 40);
			Button_SetText(MainWnd::ButtonSub, LabelString);
		}
		LastTimeChanged();
		Changed = Override;
	}

	struct VideoPacket : protected NeedParser, PacketBase {
		BOOL Read(BufferedFile& File) {
			Set(File.Skip(2));
			if (!P || !P->Size || !File.Skip(P->Size)) {
				return FALSE;
			}
			Parser->SetPacket(*this);
			return Parser->GetPacketType();
		}
		BOOL Record() {
			if (Current) {
				if (LPBYTE Data = Parser->AllocPacket(*Current, P->Size)) {
					CopyMemory(Data, P->Data, P->Size);
					return TRUE;
				}
			}
			return FALSE;
		}
	};

	WORD GetFileVersion(CONST LPCTSTR FileName) {
		if (LPCTSTR Extension = PathFindExtension(FileName)) {
			WORD Version;
			ReadingFile File;
			if (!DiffMemory(Extension, _T(".ttm"), TLEN(5))) {
				if (File.Open(FileName, OPEN_EXISTING)) {
					if (File.ReadWord(Version)) {
						if (Version >= 700 && Version <= LATEST) {
							return Version;
						}
					}
				}
			}
			else if (!DiffMemory(Extension, _T(".cam"), TLEN(5))) {
				if (File.Open(FileName, OPEN_EXISTING)) {
					if (File.Skip(32)) {
						BYTE VersionPart[4];
						if (File.Read(VersionPart, 4)) {
							if (VersionPart[0] < 100 && VersionPart[1] < 10 && VersionPart[2] < 10 && !VersionPart[3]) {
								Version = VersionPart[0] * 100 + VersionPart[1] * 10 + VersionPart[2];
								if (Version >= 700 && Version <= LATEST) {
									return Version;
								}
							}
						}
					}
				}
			}
		}
		return NULL;
	}

	UINT Open(BOOL Override, CONST HWND Parent) {
		BufferedFile File;
		if (!File.Open(FileName)) {
			return ERROR_CANNOT_OPEN_VIDEO_FILE;
		}
		{
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
		}
		DWORD TotalTime;
		if (!File.ReadDword(TotalTime)) {
			return ERROR_CORRUPT_VIDEO;
		}
		VideoPacket Src;
		if (!Src.Read(File)) {
			return ERROR_CORRUPT_VIDEO;
		}
		if (!Parser->PlayerData) {
			return ERROR_CORRUPT_VIDEO;
		}
		if (!Parser->EnterGame) {//videos recorded with TTM BETA between 9.80 and 10.11 may have this buggy packet: fix them
			if (!Parser->Pending || !Src.Read(File) || !Parser->EnterGame || Parser->PlayerData) {
				return ERROR_CORRUPT_VIDEO;
			}
		}
		if (Last) {
			if (TotalTime > INFINITE - 1000 || TotalTime + 1000 > INFINITE - Last->Time) {
				return ERROR_CANNOT_APPEND;
			}
			TotalTime += Last->Time + 1000;
			Current = Last->Next = new(std::nothrow) Session(Last);
		}
		else {
			Current = Login = new(std::nothrow) Session();
		}
		if (!Src.Record() || !Parser->FixEnterGame(*Current)) {
			CancelOpen();
			return ERROR_CANNOT_OPEN_VIDEO_FILE;
		}
		MainWnd::Progress_Set(0, TotalTime);
		for (BYTE EnterGame; File.ReadByte(EnterGame); MainWnd::Progress_Set(Current->Time, TotalTime)) {
			switch (EnterGame) {
				case FALSE: {
					WORD Delay;
					if (!File.ReadWord(Delay) || Delay > TotalTime - Current->Time) {
						CancelOpen();
						return ERROR_CORRUPT_VIDEO;
					}
					if (!Src.Read(File) || Parser->EnterGame || Parser->Pending || Parser->PlayerData) {
						CancelOpen();
						return ERROR_CORRUPT_VIDEO;
					}
					Current = Current->Next = new(std::nothrow) Packet(Current, Delay);
					if (!Src.Record() || !Parser->FixTrade(*Current)) {
						CancelOpen();
						return ERROR_CANNOT_OPEN_VIDEO_FILE;
					}
				} break;
				case TRUE: {
					if (1000 > TotalTime - Current->Time) {
						CancelOpen();
						return ERROR_CORRUPT_VIDEO;
					}
					Current->EndSession();
					if (!Src.Read(File) || !Parser->EnterGame) {
						CancelOpen();
						return ERROR_CORRUPT_VIDEO;
					}
					Current = Current->Next = new(std::nothrow) Session(Current);
					if (!Src.Record() || !Parser->FixEnterGame(*Current)) {
						CancelOpen();
						return ERROR_CANNOT_OPEN_VIDEO_FILE;
					}
				} break;
				default: {
					CancelOpen();
					return ERROR_CORRUPT_VIDEO;
				}
			}
		}
		if (Current->Time != TotalTime) {
			CancelOpen();
			return ERROR_CORRUPT_VIDEO;
		}
		AfterOpen(Override);
		return NULL;
	}

	class Converter : private VideoPacket {
		DWORD LastTime;
		LPBYTE Store;
		WORD Want;
		WORD PacketSize;
	public:
		DWORD Avail;
		DWORD Time;

		Converter(): Store(LPBYTE(&PacketSize)), Want(2) {
			Current = Last;
		}
		~Converter() {
			delete[] LPBYTE(P);
		}
		Packet *First() {
			return Last ? Last->Next : Login;
		}
		BOOL Read(LPBYTE Data) {
			while (Avail >= Want) {
				CopyMemory(Store, Data, Want);
				Data += Want;
				Avail -= Want;
				if (!P) {
					if (PacketSize) {
						Store = Parser->AllocPacket(*this, PacketSize);
						if (!Store) {
							CancelOpen();
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
						CancelOpen();
						return FALSE;
					}
					if (Parser->EnterGame) {
						if (!Parser->FixEnterGame(*this)) {
							CancelOpen();
							return FALSE;
						}
						if (Current) {
							if (1000 > INFINITE - Current->Time) {
								CancelOpen();
								return FALSE;
							}
							Current->EndSession();
							if (!(Current = Current->Next = new(std::nothrow) Session(Current))) {
								CancelOpen();
								return FALSE;
							}
						}
						else {
							if (!(Current = Login = new(std::nothrow) Session())) {
								return FALSE;
							}
						}
						Current->Record(*this);
					}
					else if (Parser->Pending || Parser->PlayerData) {
						Discard(); //Pending packet, just get data (should not exist, but who knwows)
					}
					else if (First())  {
						if (!Parser->FixTrade(*this)) {
							CancelOpen();
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
							CancelOpen();
							return FALSE;
						}
						Current->Record(*this);
					}
					else {
						return FALSE; //common packet without a login packet first
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

	BYTE GetRECVersion() {
		return Tibia::Version >= 800 ? 6 : Tibia::Version >= 772 ? 5 : Tibia::Version >= 770 ? 4 : 3;
	}

	UINT SaveCAM() {
		NeedParser ToSave;
		DWORD Packets = 58;
		DWORD PacketSize = Parser->GetPacketData(*Login)->RawSize();
		if (PacketSize > 0xFFFF) {
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		DWORD Size = 16 + PacketSize;
		for (Current = Login; Current = Current->Next; Packets++) {
			if (Packets == INFINITE) {
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			PacketSize = Parser->GetPacketData(*Current)->RawSize();
			if (PacketSize > 0xFFFF || PacketSize + 10 > 0x7FFFFFFF - Size) {
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			Size += 10 + PacketSize;
		}
		BufferedFile Encoder;
		if (!Encoder.LZMA_Start(Size)) {
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		WritingFile File;
		if (!File.Open(FileName, CREATE_ALWAYS)) {
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.Write(CAM_HASH, 32) || !File.WriteByte((Tibia::Version / 100) % 100) || !File.WriteByte((Tibia::Version / 10) % 10) || !File.WriteByte(Tibia::Version % 10) || !File.WriteByte(0)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (Tibia::HostLen) { //Our little mod to allow otserver info, no player checks the hash
			if (!File.WriteDword(Tibia::HostLen + 3)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			if (!File.WriteByte(Tibia::HostLen)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			if (!File.Write(Tibia::Host, Tibia::HostLen)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			if (!File.WriteWord(Tibia::Port)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
		}
		else {
			if (!File.WriteDword(0)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
		}
		Encoder.WriteByte(GetRECVersion()); //Ignored by all players
		Encoder.WriteByte(2); // Ignored by all players, 2 means encrypted format, but without encryption
		Encoder.WriteDword(Packets);
		Current = Login;
		do {
			PacketData *Packet = Parser->GetPacketData(*Current);
			PacketSize = Packet->RawSize();
			Encoder.WriteWord(PacketSize);
			Encoder.WriteDword(Current->Time);
			Encoder.Write(Packet, PacketSize);
			Encoder.WriteDword(0); // Checksum is ignored by most players, algorithm is erratic and unknown
			MainWnd::Progress_Set(Current->Time, Last->Time);
		} while (Current = Current->Next);
		MainWnd::Progress_Start();
		if (!Encoder.LZMA_Compress(File)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		Changed = FALSE;
		return NULL;
	}
	UINT OpenCAM(BOOL Override, CONST HWND Parent) {
		BufferedFile File;
		if (!File.Open(FileName)) {
			return ERROR_CANNOT_OPEN_VIDEO_FILE;
		}
		{
			WORD Version;
			BYTE HostLen = NULL;
			LPCSTR Host = NULL;
			WORD Port = PORT;
			{
				LPBYTE Hash = File.Skip(32);
				if (!Hash) {
					return ERROR_CORRUPT_VIDEO;
				}
				LPBYTE VersionPart = File.Skip(4);
				if (!VersionPart || VersionPart[0] > 99 || VersionPart[1] > 9 || VersionPart[2] > 9 || VersionPart[3]) {
					return ERROR_CORRUPT_VIDEO;
				}
				Version = VersionPart[0] * 100 + VersionPart[1] * 10 + VersionPart[2];
				DWORD Metadata;
				if (!File.ReadDword(Metadata)) {
					return ERROR_CORRUPT_VIDEO;
				}
				if (Metadata) {
					if (!DiffMemory(Hash, CAM_HASH, 32)) { //Our little mod to allow otserver info
						if (Metadata < 4 || Metadata > 131) {
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
			}
			if (UINT Error = BeforeOpen(Override, Parent, Version, HostLen, Host, Port)) {
				return Error;
			}
		}
		if (!File.LZMA_Decompress()) {
			return ERROR_CORRUPT_VIDEO;
		}
		{
			BYTE Version;
			if (!File.ReadByte(Version)) { // fake TibiCAM version, ignore it (all other CAM recorders use 6 because >822)
				return ERROR_CORRUPT_VIDEO;
			}
			if (!File.ReadByte(Version) || Version != 2) { // all CAMs use encrypted flag, but without encryption
				return ERROR_CORRUPT_VIDEO;
			}
		}
		{
			DWORD Packets;
			if (!File.ReadDword(Packets) || Packets < 58) {
				return ERROR_CORRUPT_VIDEO;
			}
			Packets -= 57;
			Converter Src;
			for (DWORD i = 0; i < Packets; i++) {
				WORD Size;
				if (!File.ReadWord(Size)) {
					CancelOpen();
					return ERROR_CORRUPT_VIDEO;
				}
				if (!File.ReadDword(Src.Time)) {
					CancelOpen();
					return ERROR_CORRUPT_VIDEO;
				}
				LPBYTE Data = File.Skip(Size + 4); // ignore checksum, unknown algorithm, bynacam uses crc32 of wrong data
				if (!Data) {
					CancelOpen();
					return ERROR_CORRUPT_VIDEO;
				}
				if (Size) {
					Src.Avail = Size;
					if (!Src.Read(Data)) {
						return ERROR_CANNOT_OPEN_VIDEO_FILE;
					}
				}
				MainWnd::Progress_Set(i, Packets);
			}
			if (!Src.First()) {
				return ERROR_CORRUPT_VIDEO;
			}
			if (File.Skip(1)) {
				CancelOpen();
				return ERROR_CORRUPT_VIDEO;
			}
		}
		AfterOpen(Override);
		return NULL;
	}

	UINT SaveREC() {
		WritingFile File;
		if (!File.Open(FileName, CREATE_ALWAYS)) {
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.WriteByte(GetRECVersion())) { // this version control is what made me create ttm
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		if (!File.WriteByte(1)) { // there is no point in saving encrypted rec files anymore, and they are slower
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		DWORD Packets = 1;
		for (Current = Login; Current = Current->Next; Packets++) {
			if (Packets == INFINITE) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
		}
		if (!File.WriteDword(Packets)) {
			File.Delete(FileName);
			return ERROR_CANNOT_SAVE_VIDEO_FILE;
		}
		NeedParser ToSave;
		Current = Login;
		do {
			PacketData* Packet = Parser->GetPacketData(*Current);
			DWORD PacketSize = Packet->RawSize();
			if (!File.WriteDword(PacketSize)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			if (!File.WriteDword(Current->Time)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			if (!File.Write(Packet, PacketSize)) {
				File.Delete(FileName);
				return ERROR_CANNOT_SAVE_VIDEO_FILE;
			}
			MainWnd::Progress_Set(Current->Time, Last->Time);
		} while (Current = Current->Next);
		Changed = FALSE;
		return NULL;
	}
	WORD GuessVersion(CONST BYTE Version, CONST BYTE Encryption) {
		switch (Version) { //TODO: Guess version by packet contents
			case 3:	switch (Encryption) {
				case 1:	return 721;
				case 2: return 730;
			}
			case 4: return 770;
			case 5: return 772;
			case 6: return 800;
		}
		return 1100;
	}
	UINT OpenREC(CONST HWND Parent) {
		BufferedFile File;
		if (!File.Open(FileName)) {
			return ERROR_CANNOT_OPEN_VIDEO_FILE;
		}
		BYTE Version;
		if (!File.ReadByte(Version) || Version < 3 || Version > 6) { //3: > 7.2, 4: > 7.7, 5: > 7.72, 6: > 8.0
			return ERROR_CORRUPT_VIDEO;
		}
		BYTE Encryption;
		if (!File.ReadByte(Encryption) || !Encryption || Encryption > 2) {
			return ERROR_CORRUPT_VIDEO;
		}
		if (!Last && !Tibia::Running) {
			Tibia::SetHost(GuessVersion(Version, Encryption), NULL, LPCTSTR(NULL), PORT);
			if (!Loader().Run(Parent, TITLE_LOADER_OVERRIDE)) {
				return ERROR_TIBICAM_VERSION;
			}
		}
		DWORD Packets;
		Converter Src;
		if (Encryption == 2) {
			if (!File.ReadDword(Packets) || Packets < 58) {
				return ERROR_CORRUPT_VIDEO;
			}
			Packets -= 57;
			DWORD Mod = Version < 4 ? 5 : Version < 6 ? 8 : 6;
			for (DWORD i = 0; i < Packets; i++) {
				WORD Size;
				if (!File.ReadWord(Size) || (Version > 4 && Size & 0xF)) {
					CancelOpen();
					return ERROR_CORRUPT_VIDEO;
				}
				if (!File.ReadDword(Src.Time)) {
					CancelOpen();
					return ERROR_CORRUPT_VIDEO;
				}
				if (!Size) {
					if (!File.ReadDword(Src.Avail) || Src.Avail != 1) {
						CancelOpen();
						return ERROR_CORRUPT_VIDEO;
					}
				}
				else {
					LPBYTE Data = File.Skip(Size);
					if (!Data) {
						CancelOpen();
						return ERROR_CORRUPT_VIDEO;
					}
					if (!File.ReadDword(Src.Avail) || Src.Avail != Adler32(Data, Data + Size)) {
						CancelOpen();
						return ERROR_CORRUPT_VIDEO;
					}
					BYTE Key = Size + Src.Time + 2;
					for (WORD i = 0; i < Size; i++) {
						CHAR Minus = Key + 33 * i;
						if (Minus < 0) {
							while (-Minus % Mod) Minus++;
						}
						else {
							while (Minus % Mod) Minus++;
						}
						Data[i] -= Minus;
					}
					if (Version > 4) {
						if (!(Size = Aes256::decrypt(LPBYTE("Thy key is mine © 2006 GB Monaco"), Data, Size))) {
							CancelOpen();
							return ERROR_CORRUPT_VIDEO;
						}
					}
					Src.Avail = Size;
					if (!Src.Read(Data)) {
						return ERROR_CANNOT_OPEN_VIDEO_FILE;
					}
					MainWnd::Progress_Set(i, Packets);
				}
			}
		}
		else {
			if (!File.ReadDword(Packets) || !Packets) {
				return ERROR_CORRUPT_VIDEO;
			}
			for (DWORD i = 0; i < Packets; i++) {
				if (!File.ReadDword(Src.Avail)) {
					CancelOpen();
					return ERROR_CORRUPT_VIDEO;
				}
				if (!File.ReadDword(Src.Time)) {
					CancelOpen();
					return ERROR_CORRUPT_VIDEO;
				}
				if (Src.Avail) {
					LPBYTE Data = File.Skip(Src.Avail);
					if (!Data) {
						CancelOpen();
						return ERROR_CORRUPT_VIDEO;
					}
					if (!Src.Read(Data)) {
						return ERROR_CANNOT_OPEN_VIDEO_FILE;
					}
				}
				MainWnd::Progress_Set(i, Packets);
			}
		}
		if (!Src.First()) {
			return ERROR_CORRUPT_VIDEO;
		}
		if (File.Skip(1)) {
			CancelOpen();
			return ERROR_CORRUPT_VIDEO;
		}
		AfterOpen(TRUE);
		return NULL;
	}

	DWORD DetectFormat() {
		if (LPCTSTR Extension = PathFindExtension(FileName)) {
			if (!DiffMemory(Extension, _T(".ttm"), TLEN(5))) {
				return FILETYPE_TTM;
			}
			if (!DiffMemory(Extension, _T(".cam"), TLEN(5))) {
				return FILETYPE_CAM;
			}
			if (!DiffMemory(Extension, _T(".rec"), TLEN(5))) {
				return FILETYPE_REC;
			}
			/*if (!DiffMemory(Extension, _T(".tmv"), TLEN(5))) {
				return FILETYPE_TMV;
			}
			if (!DiffMemory(Extension, _T(".byn"), TLEN(5))) {
				return FILETYPE_BYN;
			}
			if (!DiffMemory(Extension, _T(".trp"), TLEN(5))) {
				return FILETYPE_TRP;
			}
			if (!DiffMemory(Extension, _T(".tcam"), TLEN(6))) {
				return FILETYPE_TCAM;
			}
			if (!DiffMemory(Extension, _T(".xcam"), TLEN(6))) {
				return FILETYPE_XCAM;
			}
			if (!DiffMemory(Extension, _T(".recording"), TLEN(11))) {
				return FILETYPE_TIBIACAST;
			}/**/
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
		case FILETYPE_REC:
			return OpenREC(Parent);
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
			Tibia::StartURL(File);
			return;
		}
		GetFullPathName(File, MAX_PATH, FileName, NULL);
		if (!PathFileExists(FileName)) {
			FileName[0] = NULL;
			return ErrorBox(ERROR_FILE_NOT_EXISTS, TITLE_OPEN_VIDEO);
		}
		MainWnd::Progress_Start();
		if (CONST UINT Error = OpenMultiple(DetectFormat(), GetKeyState(VK_SHIFT) < 0, MainWnd::Handle)) {
			return ErrorBox(Error, TITLE_OPEN_VIDEO);
		} //TODO: open exe, detect custom client and import accordingly
		MainWnd::Done();
		SetFileTitle();
		Tibia::AutoPlay();
		MainWnd::Progress_Stop();
	}
	VOID OpenDrop(CONST HDROP Drop) {
		BOOL Override = GetKeyState(VK_SHIFT) < 0;
		MainWnd::Wait();
		MainWnd::Progress_Start();
		INT SessionNumber = ListBox_GetCount(MainWnd::ListSessions);
		TCHAR FirstName[MAX_PATH];
		CopyMemory(FirstName, FileName, TLEN(MAX_PATH));
		TCHAR ErrorString[1400];
		SIZE_T Pos = 0;
		for (UINT i = 0; DragQueryFile(Drop, i, FileName, MAX_PATH); i++) {
			if (!Last) {
				CopyMemory(FirstName, FileName, TLEN(MAX_PATH));
			}
			if (CONST UINT Error = OpenMultiple(DetectFormat(), Override, MainWnd::Handle)) { //LPCSTR
				LPCTSTR ErrorFile = PathFindFileName(FileName);
				SIZE_T FileSize = _tcslen(ErrorFile);
				if (Pos) {
					if (Pos + FileSize + 4 + LoadStringSize(NULL, Error) > 1400) {
						continue;
					}
					ErrorString[Pos++] = '\n';
					ErrorString[Pos++] = '\n';
				}
				CopyMemory(ErrorString + Pos, ErrorFile, TLEN(FileSize));
				ErrorString[(Pos += FileSize)++] = '\n';
				Pos += LoadString(NULL, Error, ErrorString + Pos, 1400 - Pos);
				if (Error == ERROR_TIBICAM_VERSION) {
					break;
				}
			}
			else {
				SHAddToRecentDocs(SHARD_PATH, FileName);
			}
		}
		MainWnd::Done();
		CopyMemory(FileName, FirstName, TLEN(MAX_PATH));
		if (Pos) {
			MainWnd::Progress_Error();
			TCHAR TitleString[50];
			LoadString(NULL, TITLE_OPEN_VIDEO, TitleString, 50);
			MessageBox(MainWnd::Handle, ErrorString, TitleString, MB_ICONSTOP);
		}
		if (Last) {
			SetFileTitle();
			if (ListBox_GetCount(MainWnd::ListSessions) > SessionNumber) {
				ListBox_SetCurSel(MainWnd::ListSessions, SessionNumber);
				Tibia::AutoPlay();
			}
		}
		MainWnd::Progress_Stop();
	}

	BOOL GetDesktop(CONST LPCTSTR Append) {
		if (SHGetSpecialFolderPath(NULL, FileName, CSIDL_DESKTOPDIRECTORY, TRUE)) {
			PathAppend(FileName, Append);
			return TRUE;
		}
		GetFullPathName(Append, MAX_PATH, FileName, NULL);
		return FALSE;
	}
	UINT_PTR CALLBACK FileDialogHook(HWND Dialog, UINT Message, WPARAM Wp, LPARAM Lp) {
		switch (Message) {
			case WM_NOTIFY:
				switch (LPNMHDR(Lp)->code) {
					case CDN_INITDONE:
						if (!Last) {
							TCHAR Overrride[40];
							LoadString(NULL, TITLE_LOADER_OVERRIDE, Overrride, 40);
							CommDlg_OpenSave_SetControlText(GetParent(Dialog), chx1, Overrride);
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
							SetWindowLongPtr(Dialog, DWLP_MSGRESULT, TRUE);
							return TRUE;
						}
						else {
							SetFileTitle();
						}
						break;
				}
				break;
		}
		return FALSE;
	}

	VOID FileDialog() {
		TCHAR Formats[300];
		SIZE_T Pos;
		CopyMemory(Formats + (Pos = LoadString(NULL, FILETYPE_TTM, Formats, 235)), _T(" (*.ttm)\0*.ttm"), TLEN(15));
		CopyMemory(Formats + (Pos += LoadString(NULL, FILETYPE_CAM, Formats + (Pos += 15), 235 - Pos)), _T(" (*.cam)\0*.cam"), TLEN(15));
		CopyMemory(Formats + (Pos += LoadString(NULL, FILETYPE_REC, Formats + (Pos += 15), 235 - Pos)), _T(" (*.rec)\0*.rec"), TLEN(15));
		CopyMemory(Formats + (Pos += LoadString(NULL, FILETYPE_ALL, Formats + (Pos += 15), 235 - Pos)), _T("\0*.ttm;*.cam;*.rec"), TLEN(19));
		Formats[Pos + 19] = NULL;
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
		OpenFileName.Flags = OFN_ENABLESIZING | OFN_EXPLORER | OFN_ENABLEHOOK;
		OpenFileName.FlagsEx = 0;
		if (Last) {
			LoadString(NULL, TITLE_SAVE_VIDEO, Title, 20);
			OpenFileName.nFilterIndex = FILETYPE_TTM;
			OpenFileName.lCustData = LPARAM(SaveMultiple);
			OpenFileName.Flags |= OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOTESTFILECREATE | OFN_PATHMUSTEXIST;
			PathRenameExtension(FileName, _T(".ttm"));
			GetSaveFileName(&OpenFileName);
		}
		else {
			LoadString(NULL, TITLE_OPEN_VIDEO, Title, 20);
			OpenFileName.nFilterIndex = FILETYPE_ALL;
			OpenFileName.lCustData = LPARAM(OpenMultiple);
			OpenFileName.Flags |= OFN_FILEMUSTEXIST;
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
			Current = Login;
			return 0;
		}
		Current = (Packet *) ListBox_GetItemData(MainWnd::ListSessions, LoginNumber);
		return LoginNumber;
	}

	VOID SessionTimeChanged(CONST INT LoginNumber) {
		LastTimeChanged();
		SetWindowRedraw(MainWnd::ListSessions, FALSE);
		ListBox_ResetContent(MainWnd::ListSessions);
		FillSessionList();
		ListBox_SetCurSel(MainWnd::ListSessions, LoginNumber);
		SetWindowRedraw(MainWnd::ListSessions, TRUE);
	}
	Session *UnloadSession(CONST INT LoginNumber) {
		if (Session *&NextLogin = (CurrentLogin->Prev ? *(Session **) &CurrentLogin->Prev->Next : Login) = (Session *) CurrentLogin->Last->Next) {
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
		Changed = TRUE;
		SessionTimeChanged(LoginNumber - 1);
		return Last->Login;
	}

	VOID UnloadClose() {
		Current = Login;
		Login = NULL;
		Last = NULL;
		Unload();
		Changed = FALSE;
		TimeStr::SetTimeSeconds(0);
		ClearFileTitle();
		Tibia::CloseVersionMenu();
		ListBox_ResetContent(MainWnd::ListSessions);
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
			if (Login->Last->Next) {
				UnloadSession(GetSession());
			}
			else {
				UnloadClose();
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
			if (Login->Last->Next) {
				UnloadSession(GetSession());
			}
			else {
				UnloadClose();
			}
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
		}
	}

	VOID Record() {
		if (Last && 1000 > INFINITE - Last->Time || !Parser->FixEnterGame(Proxy::Server)) {
			return Proxy::Server.Discard();
		}
		if (!(Current = Last ? Last->Next = new(std::nothrow) Session(Last) : Login = new(std::nothrow) Session())) {
			return Proxy::Server.Discard();
		}
		Timer::Start();
		Current->Record(Proxy::Server);
		State = RECORD;
		TimeStr::SetTime(Current->Time);
		SetWindowRedraw(MainWnd::ListSessions, FALSE);
		ListBox_SetCurSel(MainWnd::ListSessions, ListBox_AddString(MainWnd::ListSessions, Parser->Character->Name.Data));
		ListBox_Enable(MainWnd::ListSessions, FALSE);
		SetWindowRedraw(MainWnd::ListSessions, TRUE);
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
			SetWindowRedraw(MainWnd::ListSessions, FALSE);
			CONST INT LoginNumber = ListBox_GetCurSel(MainWnd::ListSessions);
			ListBox_SetItemData(MainWnd::ListSessions, ListBox_InsertString(MainWnd::ListSessions, LoginNumber, TimeStr::Set(LoginNumber, Last->Login->SessionTime(), Last->Time)), Last->Login);
			SetWindowRedraw(MainWnd::ListSessions, TRUE);
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
			ListBox_SetCurSel(MainWnd::ListSessions, 0);
			ListBox_Enable(MainWnd::ListSessions, TRUE);
			SetWindowRedraw(MainWnd::ListSessions, TRUE);
			LoadString(NULL, LABEL_TOTAL_TIME, LabelString, 40);
			Static_SetText(MainWnd::LabelTime, LabelString);
			Static_SetText(MainWnd::StatusTime, TimeStr::Time);
			LoadString(NULL, BUTTON_SAVE, LabelString, 40);
			Button_SetText(MainWnd::ButtonSub, LabelString);
		}
		else {
			Login = NULL;
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
		ListBox_SetCurSel(MainWnd::ListSessions, 0);
		ListBox_Enable(MainWnd::ListSessions, TRUE);
		SetWindowRedraw(MainWnd::ListSessions, TRUE);
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
			Current = Login;
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
		Proxy::HandleClientClose();
		Speed = 0;
		KillTimer(MainWnd::Handle, IDTIMER);
		SetThreadExecutionState(ES_CONTINUOUS);
		Button_Enable(MainWnd::ButtonSlowDown, FALSE);
		Button_Enable(MainWnd::ButtonSpeedUp, FALSE);
		ScrollBar_Enable(MainWnd::ScrollPlayed, FALSE);
	}

	VOID Eject() {
		State = IDLE;
		Parser->RewindVideo();
		TimeStr::SetTime(Last->Time);
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
	VOID Logout() {
		State = WAIT;
		Parser->RewindVideo();
		TimeStr::SetTime(Last->Time);
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
	VOID PlayClose() {
		State = WAIT;
		Proxy::Server.Discard(); //ping packet is saved there, do not full rewind because we will unload the movie
		UnloadClose();
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
			Current = Login;
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
		if (!Login->Last->Next) {
			return PlayClose();
		}
		Current = UnloadSession(StartSkipSession());
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
			Current = Login;
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
		if (!Login->Last->Next) {
			return PlayClose();
		}
		Current = UnloadSession(GetSession());
		ScrollSession();
	}

	VOID ForcePause() {
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
		SetPlayed();
		ForcePause();
		Current = Current->Login;
		if (!SyncOne()) {
			return Logout();
		}
	}
	VOID PlayLastPacket() {
		PlayedTime = Current->Login->Last->Time;
		SetPlayed();
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
		SetPlayed();
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
		SetPlayed();
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
		SetPlayed();
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
		SetPlayed();
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
		SetPlayed();
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
		SetPlayed();
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
