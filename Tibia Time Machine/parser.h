#pragma once
#include "framework.h"
#include "file.h"
#include "packet.h"
#include "packetstring.h"
#include "zlib.h"

#define ID_LOGIN_INFO 0x0B
#define ID_LOGIN_ERROR Tibia::Version >= 1076 ? 0x0B : 0x0A
#define ID_GAME_INFO 0x15
#define ID_GAME_ERROR 0x14

namespace Direction {
	enum TYPE : BYTE {
		UP = 0,
		RIGHT = 1,
		DOWN = 2,
		LEFT = 3,
		AWRIGHT = 1, //autowalk after 7.2
		AWRIGHTUP = 2,
		AWUP = 3,
		AWLEFTUP = 4,
		AWLEFT = 5,
		AWLEFTDOWN = 6,
		AWDOWN = 7,
		AWRIGHTDOWN = 8,
		INVALID = 0xFF
	};
}

struct PDOUBLE {
	__pragma(pack(push, 1));
	BYTE Precision;
	DWORD Mantissa;
	__pragma(pack(pop));
};

class Parser700 {
protected:
	static LPBYTE Data;
	static LPBYTE End;

	BYTE Trading;
	BYTE TradeBug;

	static BOOL Avail() {
		return Data < End;
	}
	static WORD Remaining() {
		return End - Data;
	}
	static BOOL Misflow(CONST WORD Remaining) {
		return Data + Remaining != End;
	}
	static BOOL Overflow(CONST WORD Min) {
		return Data + Min > End;
	}
	static BOOL Underflow(CONST WORD Max) {
		return Data + Max < End;
	}

	static LPBYTE GetData(CONST WORD Bytes) {
		LPBYTE Result = Data;
		Data += Bytes;
		return Result;
	}
	static BYTE &GetByte() {
		return *(BYTE *) GetData(1);
	}
	static WORD &GetWord() {
		return *(WORD *) GetData(2);
	}
	static DWORD &GetDword() {
		return *(DWORD *) GetData(4);
	}
	static PDOUBLE &GetDouble() {
		return *(PDOUBLE *) GetData(5);
	}
	static PSTRING &GetString() {
		PSTRING &Result = *(PSTRING *) &GetWord();
		Data += Result.Len;
		return Result;
	}
	static PSTRING &GetString(CONST WORD Len) {
		PSTRING &Result = *(PSTRING *) &GetWord();
		Data += Result.Len = Len;
		return Result;
	}

	static VOID Read(CONST LPVOID Buffer, CONST WORD Bytes) {
		CopyMemory(Buffer, GetData(Bytes), Bytes);
	}
	static VOID Write(CONST LPCVOID Buffer, CONST WORD Bytes) {
		CopyMemory(GetData(Bytes), Buffer, Bytes);
	}
	static BOOL Compare(CONST LPCVOID Buffer, CONST WORD Bytes) {
		return !DiffMemory(Buffer, GetData(Bytes), Bytes);
	}

	static BOOL ParseSignatures();

	static VOID ConstructVideoLoginBase(CONST RSTRING &Str);

	static BOOL ParseMessage();
	static BOOL ParseUpdate();
	virtual BOOL ParseSessionKey();
	virtual BOOL ParseTokenSuccess() CONST;
	virtual BOOL ParseTokenFailure() CONST;
	virtual BOOL ParseCharacterList() CONST;

	virtual BOOL ParsePlayerData();
	static BOOL ParseCloseContainer();
	virtual BOOL ParseCloseShop() CONST;
	virtual BOOL ParseBasicData() CONST;
	virtual BOOL ParseCancelTarget() CONST;
	virtual BOOL ParseMarketClose() CONST;

	BOOL ParseMove() CONST;
	virtual Direction::TYPE GetCancelDirection(CONST Direction::TYPE AutoWalkDirection) CONST;
	BOOL ParseMove(CONST Direction::TYPE Direction) CONST;
	virtual BOOL ParseMove(CONST Direction::TYPE HorizontalDirection, CONST Direction::TYPE VerticalDirection) CONST;
	static BOOL ParseTurnUp();
	static BOOL ParseTurnRight();
	static BOOL ParseTurnDown();
	static BOOL ParseTurnLeft();
	static DWORD ParseCommand(PSTRING &Command);
	BOOL ParseSay() CONST;
	virtual BOOL ParseTarget() CONST;

	VOID ConstructCancelTarget() CONST;
	virtual VOID ConstructCancelWalk(CONST Direction::TYPE Direction) CONST;

	static BOOL ParseNumber();
	static BOOL ParseNumber(DWORD &Number, CONST DWORD Max);
	static BOOL ParseTime();
	static BOOL ParseTime(DWORD &Time);
public:
	static BYTE EnterGame;
	static BYTE PlayerData;
	static BYTE Pending;
	static DWORD PlayerID;

	DWORD Account;
	STRING Password;
	Parser700() {
		Pending = FALSE;
	}
	virtual ~Parser700() {
		Account = 0;
		Password.Wipe();
	}

	static VOID Start(CONST WORD Version);
	static VOID Stop();

	static BYTE Chars;
	static CHARACTER *Charlist;
	static CHARACTER *Character;
	static WORD MinNameLen;
	static WORD MaxNameLen;

	static BYTE GamemasterMode;

	static VOID SetPacket(CONST PacketBase& Packet) {
		End = (Data = Packet->Data) + Packet->Size;
	}
	static LPBYTE CreatePacket(PacketBase& Packet, CONST WORD Size) {
		Packet.Alloc(Size);
		if (!Packet) {
			Data = NULL;
		}
		else {
			SetPacket(Packet);
		}
		return Data;
	}

	virtual BOOL ParsePacketBase(CONST PacketBase& Packet) CONST;
	virtual LPBYTE AllocPacketBase(PacketBase& Packet, CONST WORD Size) CONST;
	virtual VOID FinishPacketBase(PacketBase& Packet) CONST;

	virtual BOOL ParsePacket(CONST PacketBase& Packet) CONST;
	virtual LPBYTE AllocPacket(PacketBase& Packet, CONST WORD Size) CONST;
	virtual VOID FinishPacket(PacketBase& Packet) CONST;

	virtual VOID RewindPacket(CONST PacketBase& Packet) CONST;

	virtual PacketData *GetPacketData(PacketBase& Src) CONST;

	virtual BOOL GetPacketType();

	static BOOL ClearChars();
	static BOOL FindChar(CONST PSTRING &Name);

	VOID ConstructMessage(CONST BYTE Type, CONST UINT ID) CONST;

	virtual BOOL ParseOutgoingLogin();
	virtual VOID ForwardLogin() CONST;

	virtual VOID ConstructVideoLogin();

	BOOL ParseIncomingLogin();

	virtual BOOL ConstructTicket() CONST;

	virtual BOOL ConstructGame() CONST;
	virtual BOOL ParseOutgoingGame();
	virtual BOOL ForwardGame() CONST;

	BOOL ParseIncomingGame();

	BOOL FixTrade(PacketBase &Src) CONST;
	virtual BOOL FixEnterGame(PacketBase& Src) CONST { return TRUE; }

	BOOL ParseIncomingReconnect();
	VOID ConstructReconnect() CONST;

	BOOL ParseSafeReconnect() CONST;

	VOID ConstructVideoPing() CONST;

	virtual VOID ConstructVideo() CONST;
	virtual VOID RewindVideo() CONST;

	//VOID ParseDebug(CONST LPCSTR Msg, CONST Packet &Src) CONST;

	BOOL ParseVideoCommand() CONST;
	VOID ConstructPlayerLight(CONST DWORD PlayerID, CONST BYTE Level) CONST;
};
class Parser710: public Parser700 {
protected:
	BOOL ParseCharacterList() CONST;
public:
	VOID ConstructVideoLogin();
};
class Parser713: public Parser710 {
protected:
	BYTE ReportBugs;

	BOOL ParsePlayerData();
};
class Parser720: public Parser713 {
protected:
	Direction::TYPE GetCancelDirection(CONST Direction::TYPE AutoWalkDirection) CONST;
	BOOL ParseMove(CONST Direction::TYPE HorizontalDirection, CONST Direction::TYPE VerticalDirection) CONST;
};
class Parser722: public Parser720 {
public:
	BOOL ConstructGame() CONST;
	BOOL ParseOutgoingGame();
};
class Parser735: public Parser722 {
protected:
	VOID ConstructCancelWalk(CONST Direction::TYPE Direction) CONST;
};
class Parser761: public Parser735 {
	static CONST DWORD EncryptionDelta = 0x9E3779B9;
	static CONST DWORD EncryptionFinalSum = 0xC6EF3720; // 32 x Delta mod max_dword
	static LPBYTE RSA_Data;
protected:
	DWORD EncryptionKey[4];

	static VOID StartRSA();
	static VOID ParseRSA();
	static VOID FinishRSA();
	static VOID ProxyRSA();

	BOOL ReadEncryptionKey() {
		if (GetByte()) {
			return FALSE;
		}
		Read(EncryptionKey, 16);
		return TRUE;
	}
	VOID WriteEncryptionKey() CONST {
		GetByte() = 0;
		Write(EncryptionKey, 16);
	}

	VOID Decrypt() CONST;
	VOID Encrypt() CONST;

public:
	BOOL ParsePacket(CONST PacketBase& Packet) CONST;
	LPBYTE AllocPacket(PacketBase &Packet, CONST WORD Size) CONST;
	VOID FinishPacket(PacketBase& Packet) CONST;

	PacketData *GetPacketData(PacketBase &Src) CONST;

	BOOL ParseOutgoingLogin();
	VOID ForwardLogin() CONST;

	BOOL ConstructGame() CONST;
	BOOL ParseOutgoingGame();
	BOOL ForwardGame() CONST;

	VOID ConstructVideo() CONST;
	VOID RewindVideo() CONST;
};
class Parser771: public Parser761 {
public:
	BOOL ConstructGame() CONST;
	BOOL ParseOutgoingGame();
};
class Parser820: public Parser771 {
protected:
	BOOL ParseCloseShop() CONST;
};
class Parser830: public Parser820 {
	static DWORD GetChecksum() {
		return adler32(1, Data, End - Data);
	}
public:
	STRING Account;
	~Parser830() {
		Account.Wipe();
	}

	BOOL ParsePacketBase(CONST PacketBase& Packet) CONST;
	LPBYTE AllocPacketBase(PacketBase& Packet, CONST WORD Size) CONST;
	VOID FinishPacketBase(PacketBase& Packet) CONST;

	VOID FinishPacket(PacketBase& Packet) CONST;

	VOID RewindPacket(CONST PacketBase& Packet) CONST;

	PacketData *GetPacketData(PacketBase &Src) CONST;

	BOOL ParseOutgoingLogin();

	BOOL ConstructGame() CONST;
	BOOL ParseOutgoingGame();
};
class Parser841: public Parser830 {
protected:
	LPBYTE Ticket;
public:
	BOOL ConstructTicket() CONST;
	BOOL ConstructGame() CONST;
	BOOL ParseOutgoingGame();
	BOOL ForwardGame() CONST;

	BOOL ParseReconnectTicket() CONST;
	BOOL ParseTicket() CONST;

	virtual VOID ConstructTicketGame(CONST LPBYTE Ticket) CONST;
};
class Parser860: public Parser841 {
protected:
	BOOL ParseCancelTarget() CONST;

	BOOL ParseTarget() CONST;

	VOID ConstructCancelTarget(CONST DWORD Count) CONST;
};
class Parser940: public Parser860 {
protected:
	BOOL ParseMarketClose() CONST;
};
class Parser950: public Parser940 {
protected:
	BOOL ParseBasicData() CONST;
};
class Parser971: public Parser950 {
protected:
	WORD ProtocolVersion;

	BOOL ParseLoginData();
	BOOL ParseCharacterList() CONST;
	virtual BOOL ParseGameData();
public:
	BOOL ParseOutgoingLogin();
	VOID ConstructVideoLogin();

	BOOL ParseOutgoingGame();

	VOID ConstructTicketGame(CONST LPBYTE Ticket) CONST;
};
class Parser980: public Parser971 {
protected:
	PDOUBLE Speed[3];

	BOOL ParsePlayerData();
public:
	BOOL GetPacketType();

	VOID ConstructEnterPendingState() CONST;

	BOOL FixEnterGame(PacketBase& Src) CONST;
};
class Parser1012: public Parser980 {
protected:
	static VOID ConstructVideoLoginBase(CONST RSTRING &Str);
	BOOL ParseCharacterListBase(CONST WORD PremDays) CONST;
	virtual BOOL ParseCharacterList() CONST;
public:
	VOID ConstructVideoLogin();
};
class Parser1054: public Parser1012 {
protected:
	BYTE CanChangePvP;

	BOOL ParsePlayerData();
public:
	BOOL FixEnterGame(PacketBase& Src) CONST;
};
class Parser1058 : public Parser1054 {
protected:
	BYTE ExpertMode;

	BOOL ParsePlayerData();
public:
	BOOL FixEnterGame(PacketBase& Src) CONST;
};
class Parser1061 : public Parser1058 {
protected:
	BOOL ParseTokenSuccess() CONST;
public:
	BOOL ParseOutgoingLogin();
};
class Parser1071 : public Parser1061 {
protected:
	WORD DatRevision;
public:
	BOOL ParseOutgoingGame();

	VOID ConstructTicketGame(CONST LPBYTE Ticket) CONST;
};
class Parser1072 : public Parser1071 {
protected:
	BOOL ParseTokenFailure() CONST;
	BOOL ParseGameData();
public:
	BOOL ParseOutgoingLogin();
	VOID ForwardLogin() CONST;

	VOID ConstructTicketGame(CONST LPBYTE Ticket) CONST;
};
class Parser1074 : public Parser1072 {
protected:
	BOOL ParseSessionKey();
	BOOL ParseGameData();
public:
	STRING SessionKey;
	~Parser1074() {
		SessionKey.Wipe();
	}

	VOID ConstructVideoLogin();
	VOID ConstructTicketGame(CONST LPBYTE Ticket) CONST;
};
class Parser1080 : public Parser1074 {
protected:
	WORD CoinPack;
	STRING Store;

	BOOL ParseCharacterList() CONST;

	BOOL ParsePlayerData();
public:
	VOID ConstructVideoLogin();

	BOOL FixEnterGame(PacketBase& Src) CONST;
};
class Parser1082 : public Parser1080 {
protected:
	BOOL ParseCharacterList() CONST;
public:
	VOID ConstructVideoLogin();
};

struct NeedParser {
	NeedParser();
	~NeedParser();
};

extern Parser700 *Parser;