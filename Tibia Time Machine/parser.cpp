#include "parser.h"
#include "tibia.h"
#include "proxy.h"
#include "video.h"
#include "bigword.h"

Parser700 *Parser;

LPBYTE Parser700::Data;
LPBYTE Parser700::End;

BYTE Parser700::Chars = 0;
CHARACTER *Parser700::Charlist;
CHARACTER *Parser700::Character;
WORD Parser700::MinNameLen;
WORD Parser700::MaxNameLen;

BYTE Parser700::GamemasterMode = FALSE;
BYTE Parser700::EnterGame;
BYTE Parser700::PlayerData;
BYTE Parser700::Pending;
DWORD Parser700::PlayerID;

LPBYTE Parser761::RSA_Data;

VOID Parser700::Start(CONST WORD Version) {
	if (Version >= 1082) { Parser = new Parser1082; } else
	if (Version >= 1080) { Parser = new Parser1080; } else
	if (Version >= 1074) { Parser = new Parser1074; } else
	if (Version >= 1072) { Parser = new Parser1072; } else
	if (Version >= 1071) { Parser = new Parser1071; } else
	if (Version >= 1061) { Parser = new Parser1061; } else
	if (Version >= 1058) { Parser = new Parser1058; } else
	if (Version >= 1054) { Parser = new Parser1054; } else
	if (Version >= 1012) { Parser = new Parser1012; } else
	if (Version >= 980) { Parser = new Parser980; }	else
	if (Version >= 971) { Parser = new Parser971; } else
	if (Version >= 950) { Parser = new Parser950; } else
	if (Version >= 940) { Parser = new Parser940; } else
	if (Version >= 860) { Parser = new Parser860; } else
	if (Version >= 841) { Parser = new Parser841; } else
	if (Version >= 830) { Parser = new Parser830; } else
	if (Version >= 820) { Parser = new Parser820; } else
	if (Version >= 771) { Parser = new Parser771; } else
	if (Version >= 761) { Parser = new Parser761; } else
	if (Version >= 735) { Parser = new Parser735; } else
	if (Version >= 722) { Parser = new Parser722; } else
	if (Version >= 720) { Parser = new Parser720; } else
	if (Version >= 713) { Parser = new Parser713; } else
	if (Version >= 710) { Parser = new Parser710; } else
	{ Parser = new Parser700; }
}
VOID Parser700::Stop() {
	delete Parser;
}

NeedParser::NeedParser() {
	if (!Tibia::Running) {
		Parser->Start(Tibia::Version);
	}
}
NeedParser::~NeedParser() {
	if (!Tibia::Running) {
		Parser->Stop();
	}
}

BOOL Parser700::ClearChars() {
	if (!Chars) {
		return FALSE;
	}
	Chars = 0;
	delete [] Charlist;
	return TRUE;
}
BOOL Parser700::FindChar(CONST PSTRING &Name) { //TODO: check 1100+ worldname
	if (!Charlist) {
		return Name == "Time Machine";
	}
	for (BYTE Char = 0; Char < Chars; Char++) {
		if (Charlist[Char].Name == Name) {
			Character = Charlist + Char;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL Parser700::ParsePacketBase(CONST PacketBase &Packet) CONST{
	SetPacket(Packet);
	return TRUE;
}
BOOL Parser830::ParsePacketBase(CONST PacketBase &Packet) CONST{
	SetPacket(Packet);
	if (Overflow(4)) {
		return FALSE;
	}
	DWORD Checksum = GetDword();
	return Checksum == GetChecksum();
}
LPBYTE Parser700::AllocPacketBase(PacketBase &Packet, CONST WORD Size) CONST{
	return CreatePacket(Packet, Size);
}
LPBYTE Parser830::AllocPacketBase(PacketBase &Packet, CONST WORD Size) CONST{
	if (CreatePacket(Packet, Size + 4)) {
		GetDword();
	}
	return Data;
}
VOID Parser700::FinishPacketBase(PacketBase &Packet) CONST{
}
VOID Parser830::FinishPacketBase(PacketBase &Packet) CONST{
	SetPacket(Packet);
	DWORD& Checksum = GetDword();
	Checksum = GetChecksum();
}

BOOL Parser700::ParsePacket(CONST PacketBase &Packet) CONST{
	SetPacket(Packet);
	return Avail();
}
BOOL Parser761::ParsePacket(CONST PacketBase &Packet) CONST{
	if (!ParsePacketBase(Packet)) {
		return FALSE;
	}
	if (Overflow(8) || Remaining() & 7) {
		return FALSE;
	}
	Decrypt();
	WORD Size = GetWord();
	if (!Size || Overflow(Size)) {
		return FALSE;
	}
	End = Data + Size;
	return TRUE;
}
LPBYTE Parser700::AllocPacket(PacketBase &Packet, CONST WORD Size) CONST{
	return CreatePacket(Packet, Size);
}
LPBYTE Parser761::AllocPacket(PacketBase &Packet, CONST WORD Size) CONST{
	if (Size > 0xFFF6) {
		return Data = End = NULL;
	}
	if (AllocPacketBase(Packet, (Size + 9) & 0xFFF8)) {
		GetWord() = Size;
		End = Data + Size;
	}
	return Data;
}
VOID Parser700::FinishPacket(PacketBase &Packet) CONST{
}
VOID Parser761::FinishPacket(PacketBase &Packet) CONST{
	SetPacket(Packet);
	Encrypt();
}
VOID Parser830::FinishPacket(PacketBase &Packet) CONST{
	SetPacket(Packet);
	DWORD &Checksum = GetDword();
	Encrypt();
	Checksum = GetChecksum();
}

VOID Parser700::RewindPacket(CONST PacketBase &Packet) CONST{
	SetPacket(Packet);
}
VOID Parser830::RewindPacket(CONST PacketBase &Packet) CONST{
	SetPacket(Packet);
	GetDword();
}

PacketData *Parser700::GetPacketData(PacketBase &Src) CONST{
	return &Src;
}
PacketData *Parser761::GetPacketData(PacketBase &Src) CONST{
	return (PacketData *)(Src->Data);
}
PacketData *Parser830::GetPacketData(PacketBase &Src) CONST{
	return (PacketData *)(Src->Data + 4);
}

VOID Parser700::ConstructMessage(CONST BYTE Type, CONST UINT ID) CONST {
	RSTRING Str(ID);
	if (Str.Len <= 1000 && AllocPacket(Proxy::Extra, 3 + Str.Len)) {
		GetByte() = Type;
		GetString(Str.Len) = Str.Data;
		FinishPacket(Proxy::Extra);
	}
}

BOOL Parser700::ParseSignatures() {
	if (GetDword() != Tibia::Signatures[0]) {
		return FALSE;
	}
	if (GetDword() != Tibia::Signatures[1]) {
		return FALSE;
	}
	if (GetDword() != Tibia::Signatures[2]) {
		return FALSE;
	}
	return TRUE;
}

BOOL Parser700::ParseOutgoingLogin() {
	SetPacket(Proxy::Client);
	if (Overflow(23) || Underflow(52)) {
		return FALSE;
	}
	if (GetByte() != 0x01) {
		return FALSE;
	}
	if (GetWord() != 2) {
		return FALSE;
	}
	if (GetWord() != Tibia::Version) {
		return FALSE;
	}
	if (!ParseSignatures()) {
		return FALSE;
	}
	DWORD NewAccount = GetDword();
	PSTRING &NewPassword = GetString();
	if (Misflow(0)) {
		return FALSE;
	}
	Password.Wipe();
	Account = NewAccount;
	Password = NewPassword;
	ClearChars();
	return TRUE;
}
BOOL Parser761::ParseOutgoingLogin() {
	SetPacket(Proxy::Client);
	if (Misflow(145)) {
		return FALSE;
	}
	if (GetByte() != 0x01) {
		return FALSE;
	}
	if (GetWord() != 2) {
		return FALSE;
	}
	if (GetWord() != Tibia::Version) {
		return FALSE;
	}
	if (!ParseSignatures()) {
		return FALSE;
	}
	ParseRSA();
	if (!ReadEncryptionKey()) {
		return FALSE;
	}
	DWORD NewAccount = GetDword();
	PSTRING &NewPassword = GetString();
	if (NewPassword.Len > 29) {
		return FALSE;
	}
	Password.Wipe();
	Account = NewAccount;
	Password = NewPassword;
	ClearChars();
	return TRUE;
}
BOOL Parser830::ParseOutgoingLogin() {
	if (!ParsePacketBase(Proxy::Client)) {
		return FALSE;
	}
	if (Misflow(145)) {
		return FALSE;
	}
	if (GetByte() != 0x01) {
		return FALSE;
	}
	if (GetWord() != 2) {
		return FALSE;
	}
	if (GetWord() != Tibia::Version) {
		return FALSE;
	}
	if (!ParseSignatures()) {
		return FALSE;
	}
	ParseRSA();
	if (!ReadEncryptionKey()) {
		return FALSE;
	}
	PSTRING &NewAccount = GetString();
	if (NewAccount.Len > 30) {
		return FALSE;
	}
	PSTRING &NewPassword = GetString();
	if (NewPassword.Len > 29) {
		return FALSE;
	}
	/*HardwareInformation = GetData(47);  8.42+*/
	//Parser700::Account = NewAccount.Len; //backwards compatibility
	Account.Wipe();
	Password.Wipe();
	Account = NewAccount;
	Password = NewPassword;
	ClearChars();
	return TRUE;
}
BOOL Parser971::ParseLoginData() {
	if (GetByte() != 0x01) {
		return FALSE;
	}
	if (GetWord() != 2) {
		return FALSE;
	}
	GetWord(); //protocol version, we just need it on the game connection
	if (GetWord() != Tibia::Version) {
		return FALSE; //versions started to get confusing past 11.00 - most errors are here
	}
	if (GetWord()) { //always zero, preview version?
		return FALSE;
	}
	if (!ParseSignatures()) {
		return FALSE;
	}
	if (GetByte()) { //is preview version?
		return FALSE;
	}
	ParseRSA();
	if (!ReadEncryptionKey()) {
		return FALSE;
	}
	PSTRING &NewAccount = GetString();
	if (NewAccount.Len > 30) {
		return FALSE;
	}
	PSTRING &NewPassword = GetString();
	if (NewPassword.Len > 29) {
		return FALSE;
	}
	Account.Wipe();
	Password.Wipe();
	Account = NewAccount;
	Password = NewPassword;
	ClearChars();
	return TRUE;
}
BOOL Parser971::ParseOutgoingLogin() {
	if (!ParsePacketBase(Proxy::Client)) {
		return FALSE;
	}
	if (Misflow(150)) {
		return FALSE;
	}
	return ParseLoginData();
}
BOOL Parser1061::ParseOutgoingLogin() {
	if (!ParsePacketBase(Proxy::Client)) {
		return FALSE;
	}
	if (Overflow(150)) { // Extra data: string about GPU and CPU capabilities - no need to parse
		return FALSE;
	}
	return ParseLoginData();
}
BOOL Parser1072::ParseOutgoingLogin() {
	if (!ParsePacketBase(Proxy::Client)) {
		return FALSE;
	}
	if (Overflow(278)) { // 10.61 extra data + 128-byte RSA block with login token (empty on 10.80+) and stay logged in flag (10.74+) - no need to parse
		return FALSE;
	}
	return ParseLoginData();
}

VOID Parser700::ForwardLogin() CONST {
}
VOID Parser761::ForwardLogin() CONST {
	FinishRSA();
	FinishPacketBase(Proxy::Client);
}
VOID Parser1072::ForwardLogin() CONST {
	FinishRSA();
	//Data = End - 128; // skip extra data and go to the last 128-byte RSA block
	//ProxyRSA(); // needed on global only
	FinishPacketBase(Proxy::Client);
}

VOID Parser700::ConstructVideoLoginBase(CONST RSTRING &Str) {
	GetByte() = 0x64;
	GetByte() = Chars = 1;
	Charlist = NULL;
	GetString(MinNameLen = MaxNameLen = 12) = "Time Machine";
	GetString(Str.Len) = Str.Data;
	GetDword() = LOCALHOST;
	GetWord() = Proxy::Game.Port;
}
VOID Parser700::ConstructVideoLogin() {
	RSTRING Str(PLAY_VIDEO);
	if (Str.Len <= 40 && CreatePacket(Proxy::Extra, 24 + Str.Len)) {
		ConstructVideoLoginBase(Str);
	}
}
VOID Parser710::ConstructVideoLogin() {
	RSTRING Str(PLAY_VIDEO);
	if (Str.Len <= 40 && AllocPacket(Proxy::Extra, 26 + Str.Len)) {
		ConstructVideoLoginBase(Str);
		GetWord() = 0; // Premium days
		FinishPacket(Proxy::Extra);
	}
}
VOID Parser971::ConstructVideoLogin() {
	RSTRING Str(PLAY_VIDEO);
	if (Str.Len <= 40 && AllocPacket(Proxy::Extra, 27 + Str.Len)) {
		ConstructVideoLoginBase(PLAY_VIDEO);
		GetByte() = FALSE;
		GetWord() = 0; // Premium days
		FinishPacket(Proxy::Extra);
	}
}
VOID Parser1012::ConstructVideoLoginBase(CONST RSTRING &Str) {
	GetByte() = 0x64;
	GetByte() = 1; //Worlds
	GetByte() = 0; //World ID
	GetString(Str.Len) = Str.Data;
	GetString(9) = "127.0.0.1";
	GetWord() = Proxy::Game.Port;
	GetByte() = FALSE; //Preview
	GetByte() = Chars = 1;
	Charlist = NULL;
	GetByte() = 0; //World ID
	GetString(MinNameLen = MaxNameLen = 12) = "Time Machine";
}
VOID Parser1012::ConstructVideoLogin() {
	RSTRING Str(PLAY_VIDEO);
	if (Str.Len <= 40 && AllocPacket(Proxy::Extra, 37 + Str.Len)) {
		ConstructVideoLoginBase(Str);
		GetWord() = 0; // Premium days
		FinishPacket(Proxy::Extra);
	}
}
VOID Parser1074::ConstructVideoLogin() {
	SessionKey.Wipe(); // if we don't send a session key, it's empty
	Parser1012::ConstructVideoLogin();
}
VOID Parser1080::ConstructVideoLogin() {
	SessionKey.Wipe();
	RSTRING Str(PLAY_VIDEO);
	if (Str.Len <= 40 && AllocPacket(Proxy::Extra, 40 + Str.Len)) {
		ConstructVideoLoginBase(Str);
		GetByte() = FALSE; //Premium
		GetDword() = 0; // Premium time
		FinishPacket(Proxy::Extra);
	}
}
VOID Parser1082::ConstructVideoLogin() {
	SessionKey.Wipe();
	RSTRING Str(PLAY_VIDEO);
	if (Str.Len <= 40 && AllocPacket(Proxy::Extra, 41 + Str.Len)) {
		ConstructVideoLoginBase(Str);
		GetByte() = FALSE; //Frozen
		GetByte() = FALSE; //Premium
		GetDword() = 0; // Premium time
		FinishPacket(Proxy::Extra);
	}
}

BOOL Parser700::ParseIncomingLogin() {
	if (!ParsePacket(Proxy::Server)) {
		return FALSE;
	}
	EnterGame = FALSE;
	do {
		switch (GetByte()) {
			case 0x0A: //"Sorry" Error // No effect on 10.76+
				if (!ParseMessage()) {
					ClearChars();
					return FALSE;
				}
				break;
			case 0x0B: //For Your Information //10:76+: "Sorry" error
				if (!ParseMessage()) {
					ClearChars();
					return FALSE;
				}
				break;
			case 0x0C:
				if (!ParseTokenSuccess()) { // 1061+: token success
					ClearChars();
					return FALSE;
				}
				break;
			case 0x0D:
				if (!ParseTokenFailure()) { // 1072+: token error
					ClearChars();
					return FALSE;
				}
				break;
			case 0x14:
				if (!ParseMessage()) {
					ClearChars();
					return FALSE;
				}
				break;
			case 0x1E:
			case 0x1F:
				if (ClearChars()) {
					return FALSE;
				}
				if (!ParseUpdate()) {
					return FALSE;
				}
				break;
			case 0x28:
				if (!ParseSessionKey()) {
					ClearChars();
					return FALSE;
				}
				break;
			case 0x64:
				if (ClearChars()) {
					return FALSE;
				}
				if (!ParseCharacterList()) {
					return FALSE;
				}
				break;
			default:
				ClearChars();
				return FALSE;
		}
	} while (Avail());
	FinishPacket(Proxy::Server);
	return TRUE;
}
BOOL Parser700::ParseMessage() {
	if (Overflow(2)) {
		return FALSE;
	}
	GetString();
	return !Overflow(0);
}
BOOL Parser700::ParseUpdate() {
	if (Misflow(5)) {
		return FALSE;
	}
	GetData(5);
	EnterGame = TRUE;
	return TRUE;
}
BOOL Parser700::ParseSessionKey() {
	return FALSE;
}
BOOL Parser1074::ParseSessionKey() {
	if (Overflow(2)) {
		return FALSE;
	}
	PSTRING &NewSessionKey = GetString();
	if (Overflow(0)) {
		return FALSE;
	}
	SessionKey.Wipe();
	SessionKey = NewSessionKey;
	return TRUE;
}
BOOL Parser700::ParseTokenSuccess() CONST {
	return FALSE;
}
BOOL Parser1061::ParseTokenSuccess() CONST {
	if (Overflow(1)) {
		return FALSE;
	}
	GetByte();
	return TRUE;
}
BOOL Parser700::ParseTokenFailure() CONST {
	return FALSE;
}
BOOL Parser1072::ParseTokenFailure() CONST {
	if (Overflow(1)) {
		return FALSE;
	}
	GetByte();
	return TRUE;
}
BOOL Parser700::ParseCharacterList() CONST {
	if (Overflow(1)) {
		return FALSE;
	}
	Chars = GetByte();
	if (Overflow(Chars * 10)) {
		Chars = 0;
		return FALSE;
	}
	Charlist = new CHARACTER[Chars];
	for (BYTE Char = 0; Char < Chars; Char++) {
		PSTRING &Name = GetString();
		if (Name.Len > 40 || Overflow(8 + (Chars - Char - 1) * 10)) {
			Chars = 0;
			delete [] Charlist;
			return FALSE;
		}
		PSTRING& World = GetString();
		if (World.Len > 40 || Overflow(6 + (Chars - Char - 1) * 10)) {
			Chars = 0;
			delete [] Charlist;
			return FALSE;
		}
		Charlist[Char].Name = Name;
		Charlist[Char].WorldName = World;
		DWORD &Host = GetDword();
		Charlist[Char].Host = Host;
		Host = LOCALHOST;
		WORD &Port = GetWord();
		Charlist[Char].Port = Port;
		Port = Proxy::Game.Port;
		if (!Char) {
			MinNameLen = MaxNameLen = Name.Len;
		}
		else if (Name.Len < MinNameLen) {
			MinNameLen = Name.Len;
		}
		else if (Name.Len > MaxNameLen) {
			MaxNameLen = Name.Len;
		}
	}
	return TRUE;
}
BOOL Parser710::ParseCharacterList() CONST {
	if (Overflow(3)) {
		return FALSE;
	}
	Chars = GetByte();
	if (Overflow(Chars * 10 + 2)) {
		Chars = 0;
		return FALSE;
	}
	Charlist = new CHARACTER[Chars];
	for (BYTE Char = 0; Char < Chars; Char++) {
		PSTRING &Name = GetString();
		if (Name.Len > 40 || Overflow(10 + (Chars - Char - 1) * 10)) {
			Chars = 0;
			delete [] Charlist;
			return FALSE;
		}
		PSTRING& World = GetString();
		if (World.Len > 40 || Overflow(8 + (Chars - Char - 1) * 10)) {
			Chars = 0;
			delete [] Charlist;
			return FALSE;
		}
		Charlist[Char].Name = Name;
		Charlist[Char].WorldName = World;
		DWORD &Host = GetDword();
		Charlist[Char].Host = Host;
		Host = LOCALHOST;
		WORD &Port = GetWord();
		Charlist[Char].Port = Port;
		Port = Proxy::Game.Port;
		if (!Char) {
			MinNameLen = MaxNameLen = Name.Len;
		}
		else if (Name.Len < MinNameLen) {
			MinNameLen = Name.Len;
		}
		else if (Name.Len > MaxNameLen) {
			MaxNameLen = Name.Len;
		}
	}
	GetWord();
	return TRUE;
}
BOOL Parser971::ParseCharacterList() CONST {
	if (Overflow(3)) {
		return FALSE;
	}
	Chars = GetByte();
	if (Overflow(Chars * 11 + 2)) {
		Chars = 0;
		return FALSE;
	}
	Charlist = new CHARACTER[Chars];
	for (BYTE Char = 0; Char < Chars; Char++) {
		PSTRING &Name = GetString();
		if (Name.Len > 40 || Overflow(10 + (Chars - Char - 1) * 11)) {
			Chars = 0;
			delete [] Charlist;
			return FALSE;
		}
		PSTRING& World = GetString();
		if (World.Len > 40 || Overflow(8 + (Chars - Char - 1) * 11)) {
			Chars = 0;
			delete [] Charlist;
			return FALSE;
		}
		Charlist[Char].Name = Name;
		Charlist[Char].WorldName = World;
		DWORD &Host = GetDword();
		Charlist[Char].Host = Host;
		Host = LOCALHOST;
		WORD &Port = GetWord();
		Charlist[Char].Port = Port;
		Port = Proxy::Game.Port;
		GetByte(); //char belongs to a preview world?
		if (!Char) {
			MinNameLen = MaxNameLen = Name.Len;
		}
		else if (Name.Len < MinNameLen) {
			MinNameLen = Name.Len;
		}
		else if (Name.Len > MaxNameLen) {
			MaxNameLen = Name.Len;
		}
	}
	GetWord();
	return TRUE;
}
BOOL Parser1012::ParseCharacterListBase(CONST WORD PremDays) CONST {
	if (Overflow(2 + PremDays)) {
		return FALSE;
	}
	BYTE Worlds = GetByte();
	if (!Worlds || Overflow(Worlds * 17 + PremDays + 1)) {
		return FALSE;
	}
	WORLD* Worldlist = new WORLD[Worlds];
	for (BYTE World = 0; World < Worlds; World++) {
		Worldlist[World].ID = GetByte();
		Worldlist[World].Name = &GetString();
		if (Worldlist[World].Name->Len > 40 || Overflow(17 + (Worlds - World - 1) * 17)) {
			delete[] Worldlist;
			return FALSE;
		}
		Worldlist[World].Host = &GetString();
		if (Worldlist[World].Host->Len < 9 || Worldlist[World].Host->Len > 127 || Overflow(4 + PremDays + (Worlds - World - 1) * 17)) {
			delete[] Worldlist;
			return FALSE;
		}
		WORD& Port = GetWord();
		Worldlist[World].Port = Port;
		Port = Proxy::Game.Port;
		GetByte();
	}
	Chars = GetByte();
	if (Overflow(Chars * 3 + PremDays)) {
		Chars = 0;
		delete[] Worldlist;
		return FALSE;
	}
	Charlist = new CHARACTER[Chars];
	for (BYTE Char = 0; Char < Chars; Char++) {
		BYTE WorldID = GetByte();
		PSTRING &Name = GetString();
		if (Name.Len > 40 || Overflow(PremDays + (Chars - Char - 1) * 3)) {
			Chars = 0;
			delete[] Charlist;
			delete[] Worldlist;
			return FALSE;
		}
		Charlist[Char].Name = Name;
		Charlist[Char].Host = INADDR_ANY;
		for (BYTE World = 0; World < Worlds; World++) {
			if (Worldlist[World].ID == WorldID) {
				Charlist[Char].WorldName = *Worldlist[World].Name;
				Charlist[Char].HostName = *Worldlist[World].Host;
				Charlist[Char].Port = Worldlist[World].Port;
				break;
			}
		}
		if (!Charlist[Char].WorldName.Data || !Charlist[Char].HostName.Data) {
			Chars = 0;
			delete[] Charlist;
			delete[] Worldlist;
			return FALSE;
		}
		if (!Char) {
			MinNameLen = MaxNameLen = Name.Len;
		}
		else if (Name.Len < MinNameLen) {
			MinNameLen = Name.Len;
		}
		else if (Name.Len > MaxNameLen) {
			MaxNameLen = Name.Len;
		}
	}
	while (Worlds--) {
		Worldlist[Worlds].Host->Replace("127.0.0.1", 10);
	}
	delete[] Worldlist;
	GetData(PremDays);
	return TRUE;
}
BOOL Parser1012::ParseCharacterList() CONST{
	return ParseCharacterListBase(2);
}
BOOL Parser1080::ParseCharacterList() CONST {
	return ParseCharacterListBase(5);
}
BOOL Parser1082::ParseCharacterList() CONST {
	return ParseCharacterListBase(6);
}

BOOL Parser700::ConstructTicket() CONST {
	return FALSE;
}
BOOL Parser841::ConstructTicket() CONST {
	if (AllocPacket(Proxy::Extra, 6)) {
		GetByte() = 0x1F;
		Write(TICKET, 5);
		FinishPacketBase(Proxy::Extra);
	}
	return TRUE;
}

BOOL Parser700::ConstructGame() CONST {
	if (CreatePacket(Proxy::Extra, 10 + Password.Len + Character->Name.Len)) {
		GetByte() = 0x0A;
		GetWord() = 2;
		GetWord() = Tibia::Version;
		GetByte() = GamemasterMode;
		GetString(Character->Name.Len) = Character->Name.Data;
		GetString(Password.Len) = Password.Data;
	}
	return TRUE;
}
BOOL Parser722::ConstructGame() CONST {
	if (CreatePacket(Proxy::Extra, 14 + Password.Len + Character->Name.Len)) {
		GetByte() = 0x0A;
		GetWord() = 2;
		GetWord() = Tibia::Version;
		GetByte() = GamemasterMode;
		GetDword() = Account;
		GetString(Character->Name.Len) = Character->Name.Data;
		GetString(Password.Len) = Password.Data;
	}
	return TRUE;
}
BOOL Parser761::ConstructGame() CONST {
	if (CreatePacket(Proxy::Extra, 129)) {
		GetByte() = 0x0A;
		StartRSA();
		WriteEncryptionKey();
		GetByte() = GamemasterMode;
		GetDword() = Account;
		GetString(Character->Name.Len) = Character->Name.Data;
		GetString(Password.Len) = Password.Data;
		FinishRSA();
	}
	return TRUE;
}
BOOL Parser771::ConstructGame() CONST {
	if (CreatePacket(Proxy::Extra, 133)) {
		GetByte() = 0x0A;
		GetWord() = 2;
		GetWord() = Tibia::Version;
		StartRSA();
		WriteEncryptionKey();
		GetByte() = GamemasterMode;
		GetDword() = Account;
		GetString(Character->Name.Len) = Character->Name.Data;
		GetString(Password.Len) = Password.Data;
		FinishRSA();
	}
	return TRUE;
}
BOOL Parser830::ConstructGame() CONST {
	if (AllocPacketBase(Proxy::Extra, 133)) {
		GetByte() = 0x0A;
		GetWord() = 2;
		GetWord() = Tibia::Version;
		StartRSA();
		WriteEncryptionKey();
		GetByte() = GamemasterMode;
		GetString(Account.Len) = Account.Data;
		GetString(Character->Name.Len) = Character->Name.Data;
		GetString(Password.Len) = Password.Data;
		FinishRSA();
		FinishPacketBase(Proxy::Extra);
	}
	return TRUE;
}
BOOL Parser841::ConstructGame() CONST {
	return FALSE;
}

BOOL Parser700::ParseOutgoingGame() {
	SetPacket(Proxy::Client);
	if (Overflow(10 + Password.Len + MinNameLen) || Underflow(10 + Password.Len + MaxNameLen)) {
		return FALSE;
	}
	if (GetByte() != 0x0A) {
		return FALSE;
	}
	if (GetWord() != 2) {
		return FALSE;
	}
	if (GetWord() != Tibia::Version) {
		return FALSE;
	}
	BYTE &Gamemaster = GetByte();
	if (Gamemaster != TRUE) {
		return FALSE;
	}
	if (!GamemasterMode) {
		Gamemaster = FALSE;
	}
	PSTRING &Name = GetString();
	if (Misflow(2 + Password.Len)) {
		return FALSE;
	}
	if (!FindChar(Name)) {
		return FALSE;
	}
	return Password == GetString();
}
BOOL Parser722::ParseOutgoingGame() {
	SetPacket(Proxy::Client);
	if (Overflow(14 + Password.Len + MinNameLen) || Underflow(14 + Password.Len + MaxNameLen)) {
		return FALSE;
	}
	if (GetByte() != 0x0A) {
		return FALSE;
	}
	if (GetWord() != 2) {
		return FALSE;
	}
	if (GetWord() != Tibia::Version) {
		return FALSE;
	}
	BYTE &Gamemaster = GetByte();
	if (Gamemaster != TRUE) {
		return FALSE;
	}
	if (!GamemasterMode) {
		Gamemaster = FALSE;
	}
	if (GetDword() != Account) {
		return FALSE;
	}
	PSTRING &Name = GetString();
	if (Misflow(2 + Password.Len)) {
		return FALSE;
	}
	if (!FindChar(Name)) {
		return FALSE;
	}
	return Password == GetString();
}
BOOL Parser761::ParseOutgoingGame() {
	SetPacket(Proxy::Client);
	if (Misflow(129)) {
		return FALSE;
	}
	if (GetByte() != 0x0A) {
		return FALSE;
	}
	ParseRSA();
	if (!ReadEncryptionKey()) {
		return FALSE;
	}
	if (GetWord() != 2) {
		return FALSE;
	}
	if (GetWord() != Tibia::Version) {
		return FALSE;
	}
	BYTE &Gamemaster = GetByte();
	if (Gamemaster != TRUE) {
		return FALSE;
	}
	if (!GamemasterMode) {
		Gamemaster = FALSE;
	}
	if (GetDword() != Account) {
		return FALSE;
	}
	PSTRING &Name = GetString();
	if (Name.Len < MinNameLen || Name.Len > MaxNameLen) {
		return FALSE;
	}
	if (!FindChar(Name)) {
		return FALSE;
	}
	return Password == GetString();
}
BOOL Parser771::ParseOutgoingGame() {
	SetPacket(Proxy::Client);
	if (Misflow(133)) {
		return FALSE;
	}
	if (GetByte() != 0x0A) {
		return FALSE;
	}
	if (GetWord() != 2) {
		return FALSE;
	}
	if (GetWord() != Tibia::Version) {
		return FALSE;
	}
	ParseRSA();
	if (!ReadEncryptionKey()) {
		return FALSE;
	}
	BYTE &Gamemaster = GetByte();
	if (Gamemaster != TRUE) {
		return FALSE;
	}
	if (!GamemasterMode) {
		Gamemaster = FALSE;
	}
	if (GetDword() != Account) {
		return FALSE;
	}
	PSTRING &Name = GetString();
	if (Name.Len < MinNameLen || Name.Len > MaxNameLen) {
		return FALSE;
	}
	if (!FindChar(Name)) {
		return FALSE;
	}
	return Password == GetString();
}
BOOL Parser830::ParseOutgoingGame() {
	if (!ParsePacketBase(Proxy::Client)) {
		return FALSE;
	}
	if (Misflow(133)) {
		return FALSE;
	}
	if (GetByte() != 0x0A) {
		return FALSE;
	}
	if (GetWord() != 2) {
		return FALSE;
	}
	if (GetWord() != Tibia::Version) {
		return FALSE;
	}
	ParseRSA();
	if (!ReadEncryptionKey()) {
		return FALSE;
	}
	BYTE &Gamemaster = GetByte();
	if (Gamemaster != TRUE) {
		return FALSE;
	}
	if (!GamemasterMode) {
		Gamemaster = FALSE;
	}
	if (Account != GetString()) {
		return FALSE;
	}
	PSTRING &Name = GetString();
	if (Name.Len < MinNameLen || Name.Len > MaxNameLen) {
		return FALSE;
	}
	if (!FindChar(Name)) {
		return FALSE;
	}
	return Password == GetString();
}
BOOL Parser841::ParseOutgoingGame() {
	if (!Parser830::ParseOutgoingGame()) {
		return FALSE;
	}
	Ticket = Data;
	return Compare(TICKET, 5); //872 introduced the restore channels byte, it's ok to ignore it
}
BOOL Parser971::ParseGameData() {
	ParseRSA();
	if (!ReadEncryptionKey()) {
		return FALSE;
	}
	BYTE &Gamemaster = GetByte();
	if (Gamemaster != TRUE) {
		return FALSE;
	}
	if (!GamemasterMode) {
		Gamemaster = FALSE;
	}
	if (Account != GetString()) {
		return FALSE;
	}
	PSTRING &Name = GetString();
	if (Name.Len < MinNameLen || Name.Len > MaxNameLen) {
		return FALSE;
	}
	if (!FindChar(Name)) {
		return FALSE;
	}
	if (Password != GetString()) {
		return FALSE;
	}
	Ticket = Data;
	return Compare(TICKET, 5); //980 removed the restore channels byte, it's ok to ignore it
}
BOOL Parser971::ParseOutgoingGame() {
	if (!ParsePacketBase(Proxy::Client)) {
		return FALSE;
	}
	if (Misflow(138)) {
		return FALSE;
	}
	if (GetByte() != 0x0A) {
		return FALSE;
	}
	if (GetWord() != 2) {
		return FALSE;
	}
	ProtocolVersion = GetWord(); //protocol version (971 OK, 980: 972, 981: 973, 1000: 979, 1031 OK, 1033: 1032, WTF CIP?)
	if (GetWord() != Tibia::Version) {
		return FALSE;
	}
	if (GetWord() || GetByte()) { //always zero, preview version?
		return FALSE;
	}
	return ParseGameData();
}
BOOL Parser1071::ParseOutgoingGame() {
	if (!ParsePacketBase(Proxy::Client)) {
		return FALSE;
	}
	if (Misflow(140)) {
		return FALSE;
	}
	if (GetByte() != 0x0A) {
		return FALSE;
	}
	if (GetWord() != 2) {
		return FALSE;
	}
	ProtocolVersion = GetWord(); //protocol version (971 OK, 980: 972, 981: 973, 1000: 979, 1031 OK, 1033: 1032, WTF CIP?)
	if (GetWord() != Tibia::Version) {
		return FALSE;
	}
	if (GetWord()) { //always zero, preview version?
		return FALSE;
	}
	DatRevision = GetWord();
	if (GetByte()) { //always zero, preview version?
		return FALSE;
	}
	return ParseGameData();
}
BOOL Parser1072::ParseGameData() {
	ParseRSA();
	if (!ReadEncryptionKey()) {
		return FALSE;
	}
	BYTE &Gamemaster = GetByte();
	if (Gamemaster != TRUE) {
		return FALSE;
	}
	if (!GamemasterMode) {
		Gamemaster = FALSE;
	}
	if (Account != GetString()) {
		return FALSE;
	}
	PSTRING &Name = GetString();
	if (Name.Len < MinNameLen || Name.Len > MaxNameLen) {
		return FALSE;
	}
	if (!FindChar(Name)) {
		return FALSE;
	}
	if (Password != GetString()) {
		return FALSE;
	}
	if (GetWord()) { // authenticator token
		return FALSE;
	}
	Ticket = Data;
	return Compare(TICKET, 5);
}
BOOL Parser1074::ParseGameData() {
	ParseRSA();
	if (!ReadEncryptionKey()) {
		return FALSE;
	}
	BYTE &Gamemaster = GetByte();
	if (Gamemaster != TRUE) {
		return FALSE;
	}
	if (!GamemasterMode) {
		Gamemaster = FALSE;
	}
	if (SessionKey != GetString()) {
		return FALSE;
	}
	PSTRING &Name = GetString();
	if (Name.Len < MinNameLen || Name.Len > MaxNameLen) {
		return FALSE;
	}
	if (!FindChar(Name)) {
		return FALSE;
	}
	Ticket = Data;
	return Compare(TICKET, 5);
}

BOOL Parser700::ForwardGame() CONST {
	return TRUE;
}
BOOL Parser761::ForwardGame() CONST {
	FinishRSA();
	FinishPacketBase(Proxy::Client);
	return TRUE;
}
BOOL Parser841::ForwardGame() CONST {
	return FALSE;
}

BOOL Parser841::ParseReconnectTicket() CONST {
	if (!ParsePacketBase(Proxy::Extra)) {
		return FALSE;
	}
	if (Misflow(8) || GetWord() != 6) {
		return FALSE;
	}
	if (GetByte() != 0x1F) {
		return FALSE;
	}
	BYTE Ticket[5];
	Read(Ticket, 5);
	Proxy::Extra.Discard();
	ConstructTicketGame(Ticket);
	return TRUE;
}

BOOL Parser841::ParseTicket() CONST {
	if (!ParsePacketBase(Proxy::Server)) {
		return FALSE;
	}
	if (Misflow(8) || GetWord() != 6) {
		return FALSE;
	}
	if (GetByte() != 0x1F) {
		return FALSE;
	}
	Read(Ticket, 5);
	FinishRSA();
	FinishPacketBase(Proxy::Client);
	return TRUE;
}

VOID Parser841::ConstructTicketGame(CONST LPBYTE Ticket) CONST {
	if (AllocPacketBase(Proxy::Extra, 133)) {
		GetByte() = 0x0A;
		GetWord() = 2;
		GetWord() = Tibia::Version;
		StartRSA();
		WriteEncryptionKey();
		GetByte() = GamemasterMode;
		GetString(Account.Len) = Account.Data;
		GetString(Character->Name.Len) = Character->Name.Data;
		GetString(Password.Len) = Password.Data;
		Write(Ticket, 5);
		GetByte() = 0; //restore channels byte, added at 872 but harmless before it
		FinishRSA();
		FinishPacketBase(Proxy::Extra);
	}
}
VOID Parser971::ConstructTicketGame(CONST LPBYTE Ticket) CONST {
	if (AllocPacketBase(Proxy::Extra, 138)) {
		GetByte() = 0x0A;
		GetWord() = 2;
		GetWord() = ProtocolVersion;
		GetWord() = Tibia::Version;
		GetWord() = 0;
		GetByte() = FALSE;
		StartRSA();
		WriteEncryptionKey();
		GetByte() = GamemasterMode;
		GetString(Account.Len) = Account.Data;
		GetString(Character->Name.Len) = Character->Name.Data;
		GetString(Password.Len) = Password.Data;
		Write(Ticket, 5);
		GetByte() = 0; //restore channels byte, removed at 980 but harmless anyway
		FinishRSA();
		FinishPacketBase(Proxy::Extra);
	}
}
VOID Parser1071::ConstructTicketGame(CONST LPBYTE Ticket) CONST {
	if (AllocPacketBase(Proxy::Extra, 140)) {
		GetByte() = 0x0A;
		GetWord() = 2;
		GetWord() = ProtocolVersion;
		GetWord() = Tibia::Version;
		GetWord() = 0;
		GetWord() = DatRevision;
		GetByte() = FALSE;
		StartRSA();
		WriteEncryptionKey();
		GetByte() = GamemasterMode;
		GetString(Account.Len) = Account.Data;
		GetString(Character->Name.Len) = Character->Name.Data;
		GetString(Password.Len) = Password.Data;
		Write(Ticket, 5);
		FinishRSA();
		FinishPacketBase(Proxy::Extra);
	}
}
VOID Parser1072::ConstructTicketGame(CONST LPBYTE Ticket) CONST {
	if (AllocPacketBase(Proxy::Extra, 140)) {
		GetByte() = 0x0A;
		GetWord() = 2;
		GetWord() = ProtocolVersion;
		GetWord() = Tibia::Version;
		GetWord() = 0;
		GetWord() = DatRevision;
		GetByte() = FALSE;
		StartRSA();
		WriteEncryptionKey();
		GetByte() = GamemasterMode;
		GetString(Account.Len) = Account.Data;
		GetString(Character->Name.Len) = Character->Name.Data;
		GetString(Password.Len) = Password.Data;
		GetWord() = 0;
		Write(Ticket, 5);
		FinishRSA();
		FinishPacketBase(Proxy::Extra);
	}
}
VOID Parser1074::ConstructTicketGame(CONST LPBYTE Ticket) CONST {
	if (AllocPacketBase(Proxy::Extra, 140)) {
		GetByte() = 0x0A;
		GetWord() = 2;
		GetWord() = ProtocolVersion;
		GetWord() = Tibia::Version;
		GetWord() = 0;
		GetWord() = DatRevision;
		GetByte() = FALSE;
		StartRSA();
		WriteEncryptionKey();
		GetByte() = GamemasterMode;
		GetString(SessionKey.Len) = SessionKey.Data;
		GetString(Character->Name.Len) = Character->Name.Data;
		Write(Ticket, 5);
		FinishRSA();
		FinishPacketBase(Proxy::Extra);
	}
}

BOOL Parser700::ParseIncomingGame() {
	if (!ParsePacket(Proxy::Server)) {
		return FALSE;
	}
	return GetPacketType();
}

BOOL Parser700::ParseIncomingReconnect() {
	if (!ParsePacket(Proxy::Extra)) {
		return FALSE;
	}
	return GetPacketType();
}
VOID Parser700::ConstructReconnect() CONST {
	Proxy::Extra.Copy(Proxy::Server);
	if (!Proxy::Extra) {
		return;
	}
	FinishPacket(Proxy::Extra);
}

BOOL Parser700::GetPacketType() {
	EnterGame = FALSE;
	PlayerData = FALSE;
	TradeBug = FALSE;
	do {
		switch (GetByte()) {
			case 0x0A:
				Trading = FALSE;
				if (!ParsePlayerData()) {
					return FALSE;
				}
				return EnterGame = PlayerData = TRUE;;
			case 0x15:
				if (!ParseMessage()) {
					return FALSE;
				}
				break;
			case 0x6F:
				if (!ParseCloseContainer()) {
					return FALSE;
				}
				break;
			case 0x7C:
				if (!ParseCloseShop()) {
					return FALSE;
				}
				break;
			case 0x7D:
				if (Trading) {
					return TradeBug = TRUE;
				}
				Trading = TRUE;
				return TRUE;
			case 0x7F:
				Trading = FALSE;
				break;
			case 0x9F:
				if (!ParseBasicData()) {
					return FALSE;
				}
				break;
			case 0xA3:
				if (!ParseCancelTarget()) {
					return FALSE;
				}
				break;
			case 0xF7:
				if (!ParseMarketClose()) {
					return FALSE;
				}
				break;
			default:
				return TRUE;
		}
	} while (Avail());
	return TRUE;
}
BOOL Parser980::GetPacketType() {
	EnterGame = FALSE;
	PlayerData = FALSE;
	Pending = FALSE;
	TradeBug = FALSE;
	do {
		switch (GetByte()) {
			case 0xE8: //version? (seen on 1100 - gunzodus)
				Trading = FALSE;
				return EnterGame = TRUE;
			case 0x0A:
				Pending = TRUE;
				break;
			case 0x0F:
				Trading = FALSE;
				return EnterGame = TRUE;
			case 0x15:
				if (!ParseMessage()) {
					return FALSE;
				}
				break;
			case 0x17:
				if (!ParsePlayerData()) {
					return FALSE;
				}
				PlayerData = TRUE;
				break;
			case 0x7D:
				if (PlayerData || Pending) {
					return FALSE;
				}
				if (Trading) {
					return TradeBug = TRUE;
				}
				Trading = TRUE;
				return TRUE;
			case 0x7F:
				Trading = FALSE;
				if (PlayerData || Pending) {
					return FALSE;
				}
				break;
			case 0x9F:
				if (!ParseBasicData()) {
					return FALSE;
				}
				break;
			default:
				return TRUE;
		}
	} while (Avail());
	return TRUE;
}
BOOL Parser700::ParsePlayerData() {
	if (Overflow(6)) {
		return FALSE;
	}
	PlayerID = GetDword();
	return GetWord() == 0x32;
}
BOOL Parser713::ParsePlayerData() {
	if (Overflow(7)) {
		return FALSE;
	}
	PlayerID = GetDword();
	if (GetWord() != 0x32) {
		return FALSE;
	}
	ReportBugs = GetByte();
	return TRUE;
}
BOOL Parser980::ParsePlayerData() {
	if (Overflow(22)) {
		return FALSE;
	}
	PlayerID = GetDword();
	if (GetWord() != 0x32) {
		return FALSE;
	}
	Speed[0] = GetDouble();
	Speed[1] = GetDouble();
	Speed[2] = GetDouble();
	ReportBugs = GetByte();
	return TRUE;
}
BOOL Parser1054::ParsePlayerData() {
	if (Overflow(23)) {
		return FALSE;
	}
	PlayerID = GetDword();
	if (GetWord() != 0x32) {
		return FALSE;
	}
	Speed[0] = GetDouble();
	Speed[1] = GetDouble();
	Speed[2] = GetDouble();
	ReportBugs = GetByte();
	CanChangePvP = GetByte();
	return TRUE;
}
BOOL Parser1058::ParsePlayerData() {
	if (Overflow(24)) {
		return FALSE;
	}
	PlayerID = GetDword();
	if (GetWord() != 0x32) {
		return FALSE;
	}
	Speed[0] = GetDouble();
	Speed[1] = GetDouble();
	Speed[2] = GetDouble();
	ReportBugs = GetByte();
	CanChangePvP = GetByte();
	ExpertMode = GetByte();
	return TRUE;
}
BOOL Parser1080::ParsePlayerData() {
	if (Overflow(28)) {
		return FALSE;
	}
	PlayerID = GetDword();
	if (GetWord() != 0x32) {
		return FALSE;
	}
	Speed[0] = GetDouble();
	Speed[1] = GetDouble();
	Speed[2] = GetDouble();
	ReportBugs = GetByte();
	CanChangePvP = GetByte();
	ExpertMode = GetByte();
	Store = GetString();
	if (Overflow(2)) {
		return FALSE;
	}
	CoinPack = GetWord();
	return TRUE;
}
BOOL Parser700::ParseCloseContainer() {
	if (Overflow(1)) {
		return FALSE;
	}
	GetByte();
	return TRUE;
}
BOOL Parser700::ParseCloseShop() CONST {
	return FALSE;
}
BOOL Parser820::ParseCloseShop() CONST {
	return TRUE;
}
BOOL Parser700::ParseBasicData() CONST {
	return FALSE;
}
BOOL Parser950::ParseBasicData() CONST { //shit sent by otsevers, meant only to the flash client
	if (Overflow(4)) {
		return FALSE;
	}
	GetByte(); //is premium
	GetByte(); //vocation
	WORD KnownSpells = GetWord();
	if (Overflow(KnownSpells)) {
		return FALSE;
	}
	GetData(KnownSpells); //spell ids
	return TRUE;
}
BOOL Parser700::ParseCancelTarget() CONST {
	return TRUE;
}
BOOL Parser860::ParseCancelTarget() CONST {
	if (Overflow(4)) {
		return FALSE;
	}
	GetDword();
	return TRUE;
}
BOOL Parser700::ParseMarketClose() CONST {
	return FALSE;
}
BOOL Parser940::ParseMarketClose() CONST {
	return TRUE;
}

BOOL Parser700::ParseSafeReconnect() CONST {
	if (!ParsePacket(Proxy::Client)) {
		return TRUE;
	}
	if (GetByte() == 0x14) {
		return TRUE;
	}
	FinishPacket(Proxy::Client);
	return FALSE;
}

VOID Parser980::ConstructEnterPendingState() CONST {
	if (AllocPacket(Proxy::Extra, 1)) {
		GetByte() = 0x0A; //makes client re-send 0x0F, but the 0x0F response is bugged if you don't reconnect (battle list is already known)
		FinishPacket(Proxy::Extra);
	}
}

BOOL Parser700::FixTrade(PacketBase &Src) CONST {
	if (TradeBug) {
		LPBYTE Trash = LPBYTE(&Src);
		CONST PacketData *Old = GetPacketData(Src);
		if (!AllocPacket(Src, Old->Size + 1)) {
			return FALSE;
		}
		GetByte() = 0x7F;
		Write(Old->Data, Old->Size);
		delete[] Trash;
	}
	return TRUE;
}
BOOL Parser980::FixEnterGame(PacketBase& Src) CONST{
	if (!PlayerData) {
		LPBYTE Trash = LPBYTE(&Src);
		CONST PacketData *Old = GetPacketData(Src);
		if (!AllocPacket(Src, Old->Size + 24)) {
			return FALSE;
		}
		GetByte() = 0x17;
		GetDword() = PlayerID;
		GetWord() = 0x32;
		GetDouble() = Speed[0];
		GetDouble() = Speed[1];
		GetDouble() = Speed[2];
		GetByte() = ReportBugs;
		GetByte() = 0x0A;
		Write(Old->Data, Old->Size);
		delete[] Trash;
	}
	return TRUE;
}
BOOL Parser1054::FixEnterGame(PacketBase& Src) CONST {
	if (!PlayerData) {
		LPBYTE Trash = LPBYTE(&Src);
		CONST PacketData *Old = GetPacketData(Src);
		if (!AllocPacket(Src, Old->Size + 25)) {
			return FALSE;
		}
		GetByte() = 0x17;
		GetDword() = PlayerID;
		GetWord() = 0x32;
		GetDouble() = Speed[0];
		GetDouble() = Speed[1];
		GetDouble() = Speed[2];
		GetByte() = ReportBugs;
		GetByte() = CanChangePvP;
		GetByte() = 0x0A;
		Write(Old->Data, Old->Size);
		delete[] Trash;
	}
	return TRUE;
}
BOOL Parser1058::FixEnterGame(PacketBase& Src) CONST {
	if (!PlayerData) {
		LPBYTE Trash = LPBYTE(&Src);
		CONST PacketData*Old = GetPacketData(Src);
		if (!AllocPacket(Src, Old->Size + 26)) {
			return FALSE;
		}
		GetByte() = 0x17;
		GetDword() = PlayerID;
		GetWord() = 0x32;
		GetDouble() = Speed[0];
		GetDouble() = Speed[1];
		GetDouble() = Speed[2];
		GetByte() = ReportBugs;
		GetByte() = CanChangePvP;
		GetByte() = ExpertMode;
		GetByte() = 0x0A;
		Write(Old->Data, Old->Size);
		delete[] Trash;
	}
	return TRUE;
}
BOOL Parser1080::FixEnterGame(PacketBase& Src) CONST {
	if (!PlayerData) {
		LPBYTE Trash = LPBYTE(&Src);
		CONST PacketData *Old = GetPacketData(Src);
		if (!AllocPacket(Src, Old->Size + 30 + Store.Len)) {
			return FALSE;
		}
		GetByte() = 0x17;
		GetDword() = PlayerID;
		GetWord() = 0x32;
		GetDouble() = Speed[0];
		GetDouble() = Speed[1];
		GetDouble() = Speed[2];
		GetByte() = ReportBugs;
		GetByte() = CanChangePvP;
		GetByte() = ExpertMode;
		GetString(Store.Len) = Store.Data; // In-game store URL
		GetWord() = CoinPack;
		GetByte() = 0x0A;
		Write(Old->Data, Old->Size);
		delete[] Trash;
	}
	return TRUE;
}

VOID Parser700::ConstructVideoPing() CONST {
	if (AllocPacket(Proxy::Server, 1)) {
		GetByte() = 0x1E;
		FinishPacket(Proxy::Server);
	}
}

VOID Parser700::ConstructVideo() CONST {
}
VOID Parser761::ConstructVideo() CONST {
	if (Video::Current->NeedEncrypt()) {
		FinishPacket(*Video::Current);
	}
}
VOID Parser700::RewindVideo() CONST {
}
VOID Parser761::RewindVideo() CONST {
	Video::Current = Video::First;
	do {
		if (Video::Current->NeedDecrypt()) {
			RewindPacket(*Video::Current);
			Decrypt();
		}
	} while (Video::Current = Video::Current->Next);
}

BOOL Parser700::ParseVideoCommand() CONST {
	if (!ParsePacket(Proxy::Client)) {
		return FALSE;
	}
	switch (GetByte()) {
		case 0x64:
			return ParseMove();
		case 0x65:
			return ParseMove(Direction::UP);
		case 0x66:
			return ParseMove(Direction::RIGHT);
		case 0x67:
			return ParseMove(Direction::DOWN);
		case 0x68:
			return ParseMove(Direction::LEFT);
		case 0x6A:
			return ParseMove(Direction::RIGHT, Direction::UP);
		case 0x6B:
			return ParseMove(Direction::RIGHT, Direction::DOWN);
		case 0x6C:
			return ParseMove(Direction::LEFT, Direction::DOWN);
		case 0x6D:
			return ParseMove(Direction::LEFT, Direction::UP);
		case 0x6F:
			return ParseTurnUp();
		case 0x70:
			return ParseTurnRight();
		case 0x71:
			return ParseTurnDown();
		case 0x72:
			return ParseTurnLeft();
		case 0x96:
			return ParseSay();
		case 0xA1:
			return ParseTarget(); //attack
		case 0xA2:
			return ParseTarget(); //follow
	}
	return TRUE;
}
BOOL Parser700::ParseMove() CONST {
	if (Overflow(1)) {
		return FALSE;
	}
	BYTE Count = GetByte();
	if (Misflow(Count) || !Count) {
		return FALSE;
	}
	Direction::TYPE CancelDirection = GetCancelDirection(Direction::TYPE(GetByte()));
	if (CancelDirection == Direction::INVALID) {
		return FALSE;
	}
	for (--Count; Count--;) {
		if (GetCancelDirection(Direction::TYPE(GetByte())) == Direction::INVALID) {
			return FALSE;
		}
	}
	ConstructCancelWalk(CancelDirection);
	return Proxy::SendConstructed();
}
Direction::TYPE Parser700::GetCancelDirection(CONST Direction::TYPE AutoWalkDirection) CONST {
	switch (AutoWalkDirection) {
		case Direction::UP:
		case Direction::RIGHT:
		case Direction::DOWN:
		case Direction::LEFT:
			return AutoWalkDirection;
	}
	return Direction::INVALID;
}
Direction::TYPE Parser720::GetCancelDirection(CONST Direction::TYPE AutoWalkDirection) CONST {
	switch (AutoWalkDirection) {
		case Direction::AWRIGHT:
			return Direction::RIGHT;
		case Direction::AWRIGHTUP:
			return Direction::RIGHT;
		case Direction::AWUP:
			return Direction::UP;
		case Direction::AWLEFTUP:
			return Direction::LEFT;
		case Direction::AWLEFT:
			return Direction::LEFT;
		case Direction::AWLEFTDOWN:
			return Direction::LEFT;
		case Direction::AWDOWN:
			return Direction::DOWN;
		case Direction::AWRIGHTDOWN:
			return Direction::RIGHT;
	}
	return Direction::INVALID;
}
BOOL Parser700::ParseMove(CONST Direction::TYPE Direction) CONST {
	if (Misflow(0)) {
		return FALSE;
	}
	ConstructCancelWalk(Direction);
	return Proxy::SendConstructed();
}
BOOL Parser700::ParseMove(CONST Direction::TYPE HorizontalDirection, CONST Direction::TYPE VerticalDirection) CONST {
	return TRUE; //ignore like an unknown packet
}
BOOL Parser720::ParseMove(CONST Direction::TYPE HorizontalDirection, CONST Direction::TYPE VerticalDirection) CONST {
	return Parser700::ParseMove(HorizontalDirection); //we are going to cancel it anyway
}
BOOL Parser700::ParseTurnUp() {
	if (Misflow(0)) {
		return FALSE;
	}
	Video::SpeedUp();
	return TRUE;
}
BOOL Parser700::ParseTurnRight() {
	if (Misflow(0)) {
		return FALSE;
	}
	Video::SkipForward(15000);
	return TRUE;
}
BOOL Parser700::ParseTurnDown() {
	if (Misflow(0)) {
		return FALSE;
	}
	Video::SlowDown();
	return TRUE;
}
BOOL Parser700::ParseTurnLeft() {
	if (Misflow(0)) {
		return FALSE;
	}
	Video::SkipBackward(15000);
	return TRUE;
}
DWORD Parser700::ParseCommand(PSTRING &Command) {
	Command.Len = 0;
	Data = LPBYTE(Command.Data);
	while (Avail()) {
		if (GetByte() == ' ') {
			return 0;
		}
		Command.Len++;
	}
	return TRUE;
}
BOOL Parser700::ParseSay() CONST {
	if (Overflow(3)) {
		return FALSE;
	}
	if (GetByte() == 1) { //1 = talk on the console
		PSTRING &Command = GetString();
		if (Misflow(0) || Command.Len > 255) {
			return FALSE;
		}
		DWORD Param = ParseCommand(Command);
		switch (Command.Len) {
			case 0:
			case 1:
			case 2:
				break;
			case 3:
				if (Command == "end") {
					if (Param) {
						Video::SkipEnd();
					}
				}
				break;
			case 4:
				if (Command == "play") {
					if (Param) {
						Video::Resume();
					}
				}
				else if (Command == "fast") {
					if (Param) {
						Video::SpeedUp();
					}
					else if (ParseNumber(Param, 10)) {
						Video::SetSpeed(INT(Param));
					}
				}
				else if (Command == "slow") {
					if (Param) {
						Video::SlowDown();
					}
					else if (ParseNumber(Param, 4)) {
						Video::SetSpeed(-INT(Param));
					}
				}
				else if (Command == "stop") {
					if (Param) {
						Video::Logout();
					}
				}
				else if (Command == "back") {
					if (Param) {
						Video::SkipBackward(60000);
					}
					else if (ParseTime(Param) && Param) {
						Video::SkipBackward(Param);
					}
				}
				else if (Command == "skip") {
					if (Param) {
						Video::SkipForward(60000);
					}
					else if (ParseTime(Param)) {
						Video::SkipForward(Param);
					}
				}
				else if (Command == "goto") {
					if (!Param && ParseTime(Param)) {
						Video::SkipPosition(Param);
					}
				}
				else if (Command == "prev") {
					if (Param) {
						Video::SessionPrev();
					}
				}
				else if (Command == "next") {
					if (Param) {
						Video::SessionNext();
					}
				}
				else if (Command == "last") {
					if (Param) {
						Video::PlayLastPacket();
					}
				}
				break;
			case 5:
				if (Command == "pause") {
					if (Param) {
						Video::Pause();
					}
				}
				else if (Command == "start") {
					if (Param) {
						Video::SkipStart();
					}
				}
				else if (Command == "first") {
					if (Param) {
						Video::PlayFirstPacket();
					}
				}
				else if (Command == "light") {
					if (Param) {
						Video::SetLight(0xFF);
					}
					else if (ParseNumber(Param, 0xFF)) {
						Video::SetLight(BYTE(Param));
					}
				}
				break;
			case 6:
				if (Command == "delete") {
					if (Param) {
						Video::PlayDelete();
					}
					else if (Remaining() == 3 && Compare("all", 3)) {
						Video::PlayClose();
					}
				}
				break;
			case 7:
				if (Command == "session") {
					if (Param) {
						Video::SessionStart();
					}
					else if (ParseNumber(Param, INT_MAX)) {
						Video::SessionSelect(INT(Param));
					}
				}
				else if (Command == "cut-end") {
					if (Param) {
						Video::CutEnd();
					}
				}
				break;
			case 8:
				 if (Command == "add-fast") {
					if (!Param && ParseTime(Param)) {
						Video::AddFast(Param);
					}
				}
				else if (Command == "add-slow") {
					if (!Param && ParseTime(Param)) {
						Video::AddSlow(Param);
					}
				}
				else if (Command == "add-skip") {
					 if (!Param && ParseTime(Param)) {
						 Video::AddSkip(Param);
					 }
				 }
				break;
			case 9:
				if (Command == "cut-start") {
					if (Param) {
						Video::CutStart();
					}
				}
				else if (Command == "add-delay") {
					if (!Param && ParseNumber(Param, 0xFFFF)) {
						Video::AddDelay(WORD(Param));
					}
				}
				else if (Command == "add-light") {
					if (Param) {
						Video::AddLight(0xFF);
					}
					else if (ParseNumber(Param, 0xFF)) {
						Video::AddLight(BYTE(Param));
					}
				}
				break;
		}
	}
	return TRUE;
}
BOOL Parser700::ParseTarget() CONST {
	if (Misflow(4)) {
		return FALSE;
	}
	GetDword();
	ConstructCancelTarget();
	return Proxy::SendConstructed();
}
BOOL Parser860::ParseTarget() CONST {
	if (Misflow(8)) {
		return FALSE;
	}
	GetDword();
	ConstructCancelTarget(GetDword());
	return Proxy::SendConstructed();
}

VOID Parser700::ConstructCancelTarget() CONST {
	if (AllocPacket(Proxy::Extra, 1)) {
		GetByte() = 0xA3;
		FinishPacket(Proxy::Extra);
	}
}
VOID Parser860::ConstructCancelTarget(CONST DWORD Count) CONST {
	if (AllocPacket(Proxy::Extra, 5)) {
		GetByte() = 0xA3;
		GetDword() = Count;
		FinishPacket(Proxy::Extra);
	}
}
VOID Parser700::ConstructCancelWalk(CONST Direction::TYPE Direction) CONST {
	if (CreatePacket(Proxy::Extra, 1)) {
		GetByte() = 0xB5;
	}
}
VOID Parser735::ConstructCancelWalk(CONST Direction::TYPE Direction) CONST {
	if (AllocPacket(Proxy::Extra, 2)) {
		GetByte() = 0xB5;
		GetByte() = Direction;
		FinishPacket(Proxy::Extra);
	}
}
VOID Parser700::ConstructPlayerLight(CONST DWORD PlayerID, CONST BYTE Level) CONST {
	if (AllocPacket(Proxy::Extra, 7)) {
		GetByte() = 0x8D;
		GetDword() = PlayerID;
		GetByte() = Level;
		GetByte() = 0xD7;
		FinishPacket(Proxy::Extra);
	}
}

BOOL Parser700::ParseNumber() {
	while (Avail()) {
		if (!IsCharDigit(*Data)) {
			return FALSE;
		}
		GetByte();
	}
	return TRUE;
}
BOOL Parser700::ParseNumber(DWORD &Number, CONST DWORD Max) {
	while (Avail()) {
		if (!IsCharDigit(*Data)) {
			return FALSE;
		}
		BYTE Digit = CharToDigit(GetByte());
		if (Number > (Max - Digit) / 10) {
			Number = Max;
			return ParseNumber();
		}
		Number *= 10;
		Number += Digit;
	}
	return TRUE;
}
BOOL Parser700::ParseTime() {
	while (*Data == ':') {
		GetByte();
		if (ParseNumber()) {
			return TRUE;
		}
	}
	if (*Data == ',') {
		GetByte();
		return ParseNumber();
	}
	return FALSE;
}
BOOL Parser700::ParseTime(DWORD &Time) {
	if (ParseNumber(Time, INFINITE / 1000)) {
		Time *= 1000;
		return TRUE;
	}
	while (*Data == ':') {
		GetByte();
		DWORD Part = 0;
		BOOL Finish = ParseNumber(Part, INFINITE / 1000);
		if (Time > (INFINITE / 1000 - Part) / 60) {
			Time = INFINITE;
			return Finish || ParseTime();
		}
		Time *= 60;
		Time += Part;
		if (Finish) {
			Time *= 1000;
			return TRUE;
		}
	}
	Time *= 1000;
	if (*Data == ',') {
		GetByte();
		if (!Avail()) {
			return TRUE;
		}
		if (!IsCharDigit(*Data)) {
			return FALSE;
		}
		BYTE Digit = CharToDigit(GetByte());
		if (Time > INFINITE - Digit * 100) {
			Time = INFINITE;
			return ParseNumber();
		}
		Time += Digit * 100;
		if (!Avail()) {
			return TRUE;
		}
		if (!IsCharDigit(*Data)) {
			return FALSE;
		}
		Digit = CharToDigit(GetByte());
		if (Time > INFINITE - Digit * 10) {
			Time = INFINITE;
			return ParseNumber();
		}
		Time += Digit * 10;
		if (!Avail()) {
			return TRUE;
		}
		if (!IsCharDigit(*Data)) {
			return FALSE;
		}
		Digit = CharToDigit(GetByte());
		if (Time > INFINITE - Digit) {
			Time = INFINITE;
			return ParseNumber();
		}
		Time += Digit;
		return ParseNumber();
	}
	return FALSE;
}

VOID Parser761::StartRSA() {
	RSA_Data = Data;
}
VOID Parser761::ParseRSA() {
	StartRSA();
	BIGWORD Msg(RSA_Data, 32);
	Msg.PowMod(Tibia::RSA::Private, 32, Tibia::RSA::Modulus, 32);
	Msg.Export(RSA_Data, 32);
}
VOID Parser761::FinishRSA() {
	BIGWORD Msg(RSA_Data, 32);
	Msg.PowMod(Tibia::RSA::Public, Tibia::RSA::Modulus, 32); //global?
	Msg.Export(RSA_Data, 32);
}
VOID Parser761::ProxyRSA() {
	BIGWORD Msg(Data, 32);
	Msg.PowMod(Tibia::RSA::Private, 32, Tibia::RSA::Modulus, 32);
	Msg.PowMod(Tibia::RSA::Public, Tibia::RSA::Modulus, 32); //global?
	Msg.Export(Data, 32);
}

VOID Parser761::Decrypt() CONST {
	for (DWORD *Block = (DWORD *) Data; Block < (DWORD *) End; Block += 2) {
		for (DWORD Sum = EncryptionFinalSum, Pass = 0; Pass < 32; Pass++) {
			Block[1] -= (((Block[0] << 4) ^ (Block[0] >> 5)) + Block[0]) ^ (Sum + EncryptionKey[(Sum >> 11) & 3]);
			Sum -= EncryptionDelta;
			Block[0] -= (((Block[1] << 4) ^ (Block[1] >> 5)) + Block[1]) ^ (Sum + EncryptionKey[Sum & 3]);
		}
	}
}
VOID Parser761::Encrypt() CONST {
	for (DWORD *Block = (DWORD *) Data; Block < (DWORD *) End; Block += 2) {
		for (DWORD Sum = 0, Pass = 0; Pass < 32; Pass++) {
			Block[0] += (((Block[1] << 4) ^ (Block[1] >> 5)) + Block[1]) ^ (Sum + EncryptionKey[Sum & 3]);
			Sum += EncryptionDelta;
			Block[1] += (((Block[0] << 4) ^ (Block[0] >> 5)) + Block[0]) ^ (Sum + EncryptionKey[(Sum >> 11) & 3]);
		}
	}
}
