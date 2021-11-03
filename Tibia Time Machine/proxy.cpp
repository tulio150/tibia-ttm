#include "proxy.h"
#include "main.h"
#include "tibia.h"
#include "parser.h"
#include "video.h"

namespace Proxy {
	STATE State = WAITING;

	BOOL Socket::Notify(CONST UINT Msg, CONST LONG Events) CONST {
		return !WSAAsyncSelect(Soc, MainWnd::Handle, Msg, Events);
	}

	BOOL ListenSocket::Open(CONST UINT Msg) {
		Soc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (Soc != INVALID_SOCKET) {
			if (Notify(Msg, FD_ACCEPT)) {
				sockaddr_in LocalAddress;
				LocalAddress.sin_family = AF_INET;
				LocalAddress.sin_addr.s_addr = LOCALHOST;
				LocalAddress.sin_port = 0;
				INT NameLen = sizeof(LocalAddress);
				if (!bind(Soc, (sockaddr *) &LocalAddress, NameLen)) {
					if (!listen(Soc, 1)) {
						if (!getsockname(Soc, (sockaddr *) &LocalAddress, &NameLen)) {
							if (LocalAddress.sin_family == AF_INET && LocalAddress.sin_addr.s_addr == LOCALHOST) {
								Port = ntohs(LocalAddress.sin_port);
								return TRUE;
							}
						}
					}
				}
			}
			Close();
		}
		return FALSE;
	}

	BOOL ConnectionSocket::Open(CONST UINT Msg, CONST SOCKET New) {
		if (New != INVALID_SOCKET) {
			Soc = New;
			if (Notify(Msg, FD_READ | FD_CONNECT | FD_CLOSE)) {
				Received = 0;
				Sent = 0;
				return TRUE;
			}
			Socket::Close();
		}
		return FALSE;
	}
	VOID ConnectionSocket::Connect(CONST DWORD Address, CONST WORD Port) CONST {
		sockaddr_in RemoteAddress;
		RemoteAddress.sin_family = AF_INET;
		RemoteAddress.sin_addr.s_addr = Address;
		RemoteAddress.sin_port = htons(Port);

		connect(Soc, (sockaddr *) &RemoteAddress, sizeof(RemoteAddress));
	}
	DWORD ConnectionSocket::Connect(CONST LPCSTR Name, CONST WORD Port) CONST {
		hostent *Host = gethostbyname(Name);
		if (Host && Host->h_addrtype == AF_INET && Host->h_length == 4) {
			DWORD Address = *(DWORD *)Host->h_addr;
			Connect(Address, Port);
			return Address;
		}
		return INADDR_ANY;
	}
	VOID ConnectionSocket::PrepareToClose(VOID OnRead()) { //stupid winsock needs this with localhost
		if (P || Received) {
			Notify(WM_NULL, NULL);
			do {
				OnRead();
			} while (LastRecv > 0);
		}
	}
	VOID ConnectionSocket::Close() {
		Socket::Close();
		Discard();
		LastRecv = 0; //to get out of PrepareToClose loop
	}
	BOOL ConnectionSocket::Clean(CONST UINT Msg) {
		if (!Notify(WM_NULL, NULL)) {
			return FALSE;
		}
		for (ever) {
			if (GetPacket()) {
				Discard();
			}
			else if (!LastRecv) {
				return FALSE;
			}
			else if (LastRecv < 0) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					return FALSE;
				}
				else if (!P && !Received) {
					return Notify(Msg, FD_READ | FD_CLOSE);
				}
				else {
					fd_set Read = { 1, { Soc } };
					UpdateWindow(MainWnd::Handle);
					if (select(1, &Read, NULL, NULL, NULL) <= 0) {
						return FALSE;
					}
				}
			}
		}
	}
	BOOL ConnectionSocket::Switch(ConnectionSocket &Source, CONST UINT Msg) {
		closesocket(Soc);
		delete[] LPBYTE(P);
		*this = Source;
		Source.Soc = INVALID_SOCKET;
		Source.P = NULL;
		return Notify(Msg, FD_READ | FD_CLOSE);
	}
	BOOL ConnectionSocket::Hold(ConnectionSocket &Source, CONST UINT Msg) {
		Soc = Source.Soc;
		Source.Soc = INVALID_SOCKET;
		Source.Discard();
		return Notify(Msg, FD_CLOSE);
	}
	BOOL ConnectionSocket::Receive(CONST LPVOID Buffer, CONST DWORD Size) {
		LastRecv = recv(Soc, LPSTR(Buffer) + Received, Size - Received, 0);
		if (LastRecv < 0) {
			return FALSE;
		}
		Received += LastRecv;
		if (Received < Size) {
			return FALSE;
		}
		Received = 0;
		return TRUE;
	}
	BOOL ConnectionSocket::Send(CONST LPCVOID Buffer, CONST DWORD Size) {
		LastSend = send(Soc, LPCSTR(Buffer) + Sent, Size - Sent, 0);
		if (LastSend < 0) {
			return FALSE;
		}
		Sent += LastSend;
		if (Sent < Size) {
			return FALSE;
		}
		Sent = 0;
		return TRUE;
	}
	BOOL ConnectionSocket::BlockSend() {
		if (LastSend < 0) {
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
				return FALSE;
			}
			fd_set Read = { 1, { Soc } };
			fd_set Write = { 1, { Soc } };
			UpdateWindow(MainWnd::Handle); // To not look stuck
			if (select(2, &Read, &Write, NULL, NULL) <= 0) { // wait to be ready to send or recv
				return FALSE;
			}
			if (!Write.fd_count) { // not ready to send, but to recv or close; delay and retry
				if (!WaitForSingleObject(Tibia::Proc.hProcess, 1)) { //Sleep 1ms
					return FALSE; // Tibia closed, give up already
				}
			}
		}
		return TRUE;
	}
	BOOL ConnectionSocket::GetWorldname() {
		CHAR Worldname[41];
		if (Receive(Worldname, 41)) {
			return FALSE;
		}
		if (LastRecv <= 0) {
			return FALSE;
		}
		Received = 0;
		return Worldname[LastRecv - 1] == '\n';
	}
	BOOL ConnectionSocket::SendWorldname() {
		CHAR Worldname[41];
		CopyMemory(Worldname, Parser->Character->WorldName.Data, Parser->Character->WorldName.Len);
		Worldname[Parser->Character->WorldName.Len] = '\n';
		while (!Send(Worldname, Parser->Character->WorldName.Len + 1)) {
			if (!BlockSend()) {
				return FALSE;
			}
		}
		return TRUE;
	}
	BOOL ConnectionSocket::GetPacket() {
		if (!P) {
			if (Receive(&PacketSize, 2)) {
				Alloc(PacketSize);
				if (!P) {
					Socket::Close();
					LastRecv = 0;
					return FALSE;
				}
				if (!PacketSize) {
					return TRUE;
				}
			}
			return FALSE;
		}
		return Receive(P->Data, P->Size);
	}
	BOOL ConnectionSocket::SendPacket(CONST PacketBase &Packet) {
		if (!Packet) {
			return FALSE;
		}
		while (!Send(&Packet, Packet->RawSize())) {
			if (!BlockSend()) {
				return FALSE;
			}
		}
		return TRUE;
	}

	ListenSocket Login;
	ListenSocket Game;
	ConnectionSocket Client;
	ConnectionSocket Server;
	ConnectionSocket Extra;
	WORD Port = FALSE;

	BOOL Start() {
		if (!Login.Open(WM_SOCKET_LOGINSERVER)) {
			return FALSE;
		}
		if (!Game.Open(WM_SOCKET_GAMESERVER)) {
			Login.Close();
			return FALSE;
		}
		Parser->Start(Tibia::Version);
		return TRUE;
	}
	VOID Stop() {
		Parser->Stop();
		Game.Close();
		Login.Close();
	}

	VOID HandleTibiaClosed() {
		switch (State) {
			case LOGIN_CW:
				HandleClientClose(); break;
			case LOGIN_SC:
				Client->Wipe();
			case LOGIN_SW:
			case LOGIN_UP:
				HandleServerClose(); break;
			case GAME_CWW:
			case GAME_CW:
				HandleClientClose(); break;
			case GAME_SC:
			case GAME_SWT:
				Client->Wipe();
			case GAME_SW:
			case GAME_SWR:
			case GAME_PLAY:
				HandleServerClose(); break;
			case RECONNECT_SC:
			case RECONNECT_SWT:
			case RECONNECT_SW:
				HandleReconnectClose(); break;
		}
		Parser->ClearChars();
		return Stop();
	}

	BOOL SendConstructed() {
		BOOL Result = Client.SendPacket(Extra);
		Extra.Discard();
		return Result;
	}
	BOOL SendClientMessage(CONST BYTE Type, CONST UINT Error) {
		Parser->ConstructMessage(Type, Error);
		return SendConstructed();
	}

	VOID HandleClientClose() {
		Client.Close();
		State = WAITING;
	}
	VOID HandleServerClose() {
		Server.Close();
		return HandleClientClose();
	}
	VOID HandleClientError(CONST BYTE Type, CONST UINT Error) {
		SendClientMessage(Type, Error);
		return HandleClientClose();
	}
	VOID HandleServerError(CONST BYTE Type, CONST UINT Error) {
		Server.Close();
		return HandleClientError(Type, Error);
	}

	VOID HandleLoginClientConnect() {
		if (Client.Open(WM_SOCKET_CLIENT, Login.Accept())) {
			State = LOGIN_CW;
		}
	}
	VOID HandleVideoLogin() {
		Parser->ConstructVideoLogin();
		SendConstructed();
		Tibia::AutoPlayCharlist();
		return HandleClientClose();
	}
	VOID HandleOutgoingLogin() {
		if (Client.GetPacket()) {
			if (!Parser->ParseOutgoingLogin()) {
				return HandleClientClose();
			}
			if (Video::State == Video::WAIT && !Parser->Password.Len) {
				return HandleVideoLogin();
			}
			if (!Tibia::HostLen && !Proxy::Port) {
				Client->Wipe();
				return HandleClientError(ID_LOGIN_INFO, ERROR_OLD_TIBIA);
			}
			if (!Server.Open(WM_SOCKET_SERVER, socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
				Client->Wipe();
				return HandleClientError(ID_LOGIN_ERROR, ERROR_CREATE_SOCKET);
			}
			if (!Port) {
				if (!Server.Connect(Tibia::Host, Tibia::Port)) {
					Client->Wipe();
					return HandleServerError(ID_LOGIN_ERROR, ERROR_FIND_HOST);
				}
			}
			else {
				Server.Connect(LOCALHOST, Port);
			}
			State = LOGIN_SC;
		}
	}
	VOID HandleLoginServerConnect(CONST INT Error) {
		if (Error) {
			Client->Wipe();
			return HandleServerError(ID_LOGIN_ERROR, ERROR_CONNECT_HOST);
		}
		Parser->ForwardLogin();
		if (!Server.SendPacket(Client)) {
			Client->Wipe();
			return HandleServerClose();
		}
		Client->Wipe();
		Client.Discard();
		State = LOGIN_SW;
	}
	VOID HandleIncomingLogin() {
		if (Server.GetPacket()) {
			if (!Parser->ParseIncomingLogin()) {
				return HandleServerError(ID_LOGIN_ERROR, ERROR_CORRUPT_DATA);
			}
			if (Parser->EnterGame) {
				if (Tibia::HostLen) {
					return HandleServerError(ID_LOGIN_ERROR, ERROR_OTSERVER_UPDATE);
				}
				if (!Client.SendPacket(Server)) {
					return HandleServerClose();
				}
				Server.Discard();
				State = LOGIN_UP;
				return;
			}
			Client.SendPacket(Server);
			return HandleServerClose();
		}
	}
	VOID HandleIncomingUpdate() {
		if (Server.GetPacket()) {
			if (!Client.SendPacket(Server)) {
				return HandleServerClose();
			}
			Server.Discard();
		}
	}

	VOID HandleGameClientConnect() {
		if (!Parser->Chars) {
			return Game.Reject();
		}
		if (Client.Open(WM_SOCKET_CLIENT, Game.Accept())) {
			if (Tibia::Version > 1100) {
				State = GAME_CWW;
			}
			else {
				if (Parser->ConstructTicket()){
					if (!SendConstructed()) {
						return HandleClientClose();
					}
				}
				State = GAME_CW;
			}
		}
	}
	VOID HandleOutgoingGameWorldname() {
		if (!Client.GetWorldname()) {
			return HandleClientClose();
		}
		if (Parser->ConstructTicket()) {
			if (!SendConstructed()) {
				return HandleClientClose();
			}
		}
		State = GAME_CW;
	}
	VOID HandleVideoGame() {
		if (!Video::Last) {
			return HandleClientError(ID_GAME_INFO, ERROR_NO_VIDEO);
		}
		if (Video::State != Video::WAIT) {
			return HandleClientError(ID_GAME_INFO, ERROR_NOT_WAIT);
		}
		Client.Discard();
		State = VIDEO;
		Video::Play();
	}
	VOID HandleOutgoingGame() {
		if (Client.GetPacket()) {
			if (!Parser->ParseOutgoingGame()) {
				Client->Wipe();
				return HandleClientClose();
			}
			if (!Parser->Charlist) {
				return HandleVideoGame();
			}
			if (!Server.Open(WM_SOCKET_SERVER, socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
				Client->Wipe();
				return HandleClientError(ID_GAME_ERROR, ERROR_CREATE_SOCKET);
			}
			if (Parser->Character->Host) {
				Server.Connect(Parser->Character->Host, Parser->Character->Port);
			}
			else {
				if (!(Parser->Character->Host = Server.Connect(Parser->Character->HostName.Data, Parser->Character->Port))) {
					Client->Wipe();
					return HandleServerError(ID_GAME_ERROR, ERROR_FIND_HOST);
				}
			}
			State = GAME_SC;
		}
	}
	VOID HandleVideoCommand() {
		if (Client.GetPacket()) {
			if (Video::State == Video::PLAY) {
				if (!Parser->ParseVideoCommand()) {
					return Video::Logout();
				}
				if (!State) {
					return;
				}
			}
			Client.Discard();
		}
	}
	VOID HandleGameServerConnect(CONST INT Error) {
		if (Error) {
			Client->Wipe();
			return HandleServerError(ID_GAME_ERROR, ERROR_CONNECT_HOST);
		}
		if (Parser->ForwardGame()) {
			if (!Server.SendPacket(Client)) {
				Client->Wipe();
				return HandleServerClose();
			}
			Client->Wipe();
			Client.Discard();
			State = GAME_SW;
		}
		else {
			if (Tibia::Version > 1100) {
				if (!Server.SendWorldname()) {
					Client->Wipe();
					return HandleServerClose();
				}
			}
			State = GAME_SWT;
		}
	}
	VOID HandleIncomingTicket() {
		if (Server.GetPacket()) {
			if (!((Parser841 *)Parser)->ParseTicket()) {
				Client->Wipe();
				return HandleServerError(ID_GAME_ERROR, ERROR_CORRUPT_DATA);
			}
			if (!Server.SendPacket(Client)) {
				Client->Wipe();
				return HandleServerClose();
			}
			Client->Wipe();
			Server.Discard();
			Client.Discard();
			State = GAME_SW;
		}
	}
	VOID HandleIncomingGame() {
		if (Server.GetPacket()) {
			if (!Client.SendPacket(Server)) {
				return HandleServerClose();
			}
			if (!Parser->ParseIncomingGame()) {
				return HandleServerClose();
			}
			if (!Parser->PlayerData) {
				return HandleServerClose();
			}
			if (!Parser->EnterGame) {
				if (!Parser->Pending) {
					return HandleServerClose();
				}
				Server.Discard();
			}
			else {
				if (Video::State == Video::WAIT) {
					Video::Record();
				}
				else {
					Server.Discard();
				}
			}
			State = GAME_PLAY;
		}
	}

	VOID HandleOutgoingPlay() {
		if (Client.GetPacket()) {
			if (!Server.SendPacket(Client)) {
				return HandleLogout();
			}
			Client.Discard();
		}
	}
	VOID HandleIncomingPlay() {
		if (Server.GetPacket()) {
			if (!Client.SendPacket(Server)) {
				return HandleLogout();
			}
			switch (Video::State) {
				case Video::WAIT:
					if (Parser->ParseIncomingGame() && Parser->EnterGame) {
						return Video::Record();
					}
					break;
				case Video::RECORD:
					if (Parser->ParseIncomingGame()) {
						return Video::RecordNext();
					}
					break; 
			}
			Server.Discard();
		}
	}
	VOID HandleLogout() {
		HandleServerClose();
		if (Video::State == Video::RECORD) {
			Video::Continue();
		}
	}


	VOID HandleReconnect() {
		if (!Extra.Open(WM_SOCKET_RECONNECT, socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
			Parser->ConstructMessage(ID_GAME_INFO, ERROR_RECONNECT);
			if (!SendConstructed()) {
				return HandleServerClose();
			}
			return;
		}
		Extra.Connect(Parser->Character->Host, Parser->Character->Port);
		State = RECONNECT_SC;
	}

	VOID HandleReconnectClose() {
		Extra.Close();
		return HandleServerClose();
	}
	VOID HandleOutgoingPlayReconnect() {
		if (Client.GetPacket()) {
			if (!Server.SendPacket(Client)) {
				return HandleReconnectClose();
			}
			Client.Discard();
		}
	}
	VOID HandleIncomingPlayReconnect() {
		if (Server.GetPacket()) {
			if (!Client.SendPacket(Server)) {
				return HandleReconnectClose();
			}
			Server.Discard();
		}
	}

	VOID HandleReconnectError() {
		Extra.Close();
		Parser->ConstructMessage(ID_GAME_INFO, ERROR_RECONNECT);
		if (!SendConstructed()) {
			return HandleServerClose();
		}
		State = GAME_PLAY;
	}
	VOID HandleReconnectConnect(CONST INT Error) {
		if (Error) {
			return HandleReconnectError();
		}
		if (Parser->ConstructGame()) {
			if (!Extra.SendPacket(Extra)) {
				Extra->Wipe();
				return HandleReconnectError();
			}
			Extra->Wipe();
			Extra.Discard();
			State = RECONNECT_SW;
		}
		else {
			if (Tibia::Version > 1100) {
				if (!Extra.SendWorldname()) {
					return HandleReconnectError();
				}
			}
			State = RECONNECT_SWT;
		}
	}
	VOID HandleIncomingReconnectTicket() {
		if (Extra.GetPacket()) {
			if (!((Parser841 *)Parser)->ParseReconnectTicket()) {
				return HandleReconnectError();
			}
			if (!Extra.SendPacket(Extra)) {
				Extra->Wipe();
				return HandleReconnectError();
			}
			Extra->Wipe();
			Extra.Discard();
			State = RECONNECT_SW;
		}
	}
	VOID HandleIncomingReconnect() {
		if (Extra.GetPacket()) {
			if (!Parser->ParseIncomingReconnect()) {
				return HandleReconnectError();
			}
			if (!Parser->PlayerData) {
				return HandleReconnectError();
			}
			if (!Client.Clean(WM_SOCKET_CLIENT)) {
				return HandleReconnectClose();
			}
			if (!Server.Switch(Extra, WM_SOCKET_SERVER)) {
				return HandleServerError(ID_GAME_ERROR, ERROR_RECONNECT);
			}
			if (!Parser->EnterGame) {
				if (!Parser->Pending) {
					return HandleServerError(ID_GAME_ERROR, ERROR_RECONNECT);
				}
				((Parser980*)Parser)->ConstructEnterPendingState();
				if (!SendConstructed()) {
					return HandleServerClose();
				}
				Server.Discard();
			}
			else {
				Parser->ConstructReconnect();
				if (!SendConstructed()) {
					return HandleServerClose();
				}
				if (Video::State == Video::WAIT) {
					Video::Record();
				}
				else {
					Server.Discard();
				}
			}
			State = GAME_PLAY;
		}
	}
	VOID HandleReconnectSwitch() {
		if (!Server.Switch(Extra, WM_SOCKET_SERVER)) {
			return HandleServerClose();
		}
		State = GAME_SWR;
	}
	VOID HandleOutgoingSwitch() {
		if (Client.GetPacket()) {
			Client.Discard();
		}
	}
	VOID HandleIncomingSwitch() {
		if (Server.GetPacket()) {
			if (!Parser->ParseIncomingGame()) {
				return HandleServerClose();
			}
			if (!Parser->PlayerData) {
				return HandleServerClose();
			}
			if (!Client.Clean(WM_SOCKET_CLIENT)) {
				return HandleServerClose();
			}
			if (!Parser->EnterGame) {
				if (!Parser->Pending) {
					return HandleServerError(ID_GAME_ERROR, ERROR_RECONNECT);
				}
				((Parser980*)Parser)->ConstructEnterPendingState();
				if (!SendConstructed()) {
					return HandleServerClose();
				}
				Server.Discard();
			}
			else {
				Parser->ConstructReconnect();
				if (!SendConstructed()) {
					return HandleServerClose();
				}
				if (Video::State == Video::WAIT) {
					Video::Record();
				}
				else {
					Server.Discard();
				}
			}
			State = GAME_PLAY;
		}
	}
}