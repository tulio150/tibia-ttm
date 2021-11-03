#pragma once
#include "framework.h"
#include "packet.h"

#define LOCALHOST 0x0100007F

namespace Proxy {
	enum STATE: BYTE {
		WAITING, //waiting for a connection to login or game
		LOGIN_CW, //client connected to login, waiting data
		LOGIN_SC, //client logged in, connecting to login server
		LOGIN_SW, //connected to login server, waiting data
		LOGIN_UP, //connected to login server, waiting update data
		GAME_CWW, //client connected to game, waiting world name
		GAME_CW, //client connected to game, waiting data
		GAME_SC, //client logged in, connecting to game server
		GAME_SWT, //connected to game server, waiting for login ticket
		GAME_SW, //connected to game server, waiting data
		GAME_SWR, //connected to second game server, but disconnected from first one and switched
		GAME_PLAY, //connected to game server, waiting data from client and server
		RECONNECT_SC, //connecting to second game server
		RECONNECT_SWT, //connected to second game server, waiting for login ticket
		RECONNECT_SW, //connected to second game server, waiting data
		VIDEO, //playing a video, sending recorded packets to client and waiting commands
		WAIT_LOCALPROXY, //waiting for activation of a local proxy during tibia client startup
	};
	extern STATE State;

	class Socket {
	protected:
		SOCKET Soc;
	public:
		Socket(): Soc(INVALID_SOCKET) {}
		BOOL Notify(CONST UINT Msg, LONG Events) CONST;
		BOOL Check(CONST SOCKET Check) CONST {
			return Soc == Check;
		}
		VOID Close() {
			closesocket(Soc);
			Soc = INVALID_SOCKET;
		}
	};

	class ListenSocket: public Socket {
	public:
		WORD Port;

		BOOL Open(CONST UINT Msg);
		SOCKET Accept() CONST {
			return accept(Soc, NULL, NULL);
		}
		VOID Reject() CONST {
			closesocket(Accept());
		}
	};

	class ConnectionSocket: public Socket, public PacketBase {
		INT LastRecv;
		INT LastSend;
		DWORD Received;
		DWORD Sent;

		WORD PacketSize;

		BOOL Receive(CONST LPVOID Buffer, CONST DWORD Size);
		BOOL Send(CONST LPCVOID Buffer, CONST DWORD Size);
		BOOL BlockSend();
	public:
		BOOL Open(CONST UINT Msg, CONST SOCKET New);
		VOID Connect(CONST DWORD Address, CONST WORD Port) CONST;
		DWORD Connect(CONST LPCSTR Name, CONST WORD Port) CONST;
		VOID PrepareToClose(VOID OnRead());
		VOID Close();
		BOOL Clean(CONST UINT Msg);
		BOOL Switch(ConnectionSocket &Source, CONST UINT Msg);
		BOOL Hold(ConnectionSocket &Source, CONST UINT Msg);

		BOOL GetWorldname();
		BOOL SendWorldname();
		BOOL GetPacket();
		BOOL SendPacket(CONST PacketBase &Packet);
	};

	extern ListenSocket Login;
	extern ListenSocket Game;
	extern ConnectionSocket Client;
	extern ConnectionSocket Server;
	extern ConnectionSocket Extra;
	extern WORD Port;

	BOOL Start();
	VOID Stop();

	VOID HandleTibiaClosed();

	BOOL SendConstructed();
	BOOL SendClientMessage(CONST BYTE Type, CONST UINT Error);

	VOID HandleClientClose();
	VOID HandleServerClose();
	VOID HandleClientError(CONST BYTE Type, CONST UINT Error);
	VOID HandleServerError(CONST BYTE Type, CONST UINT Error);
	VOID HandleLoginClientConnect();
	VOID HandleOutgoingLogin();
	VOID HandleLoginServerConnect(CONST INT Error);
	VOID HandleIncomingLogin();
	VOID HandleIncomingUpdate();
	VOID HandleGameClientConnect();
	VOID HandleOutgoingGameWorldname();
	VOID HandleOutgoingGame();
	VOID HandleVideoCommand();
	VOID HandleGameServerConnect(CONST INT Error);
	VOID HandleIncomingTicket();
	VOID HandleIncomingGame();
	VOID HandleOutgoingPlay();
	VOID HandleIncomingPlay();
	VOID HandleLogout();
	VOID HandleReconnect();
	VOID HandleReconnectClose();
	VOID HandleIncomingPlayReconnect();
	VOID HandleOutgoingPlayReconnect();
	VOID HandleReconnectError();
	VOID HandleReconnectConnect(CONST INT Error);
	VOID HandleIncomingReconnectTicket();
	VOID HandleIncomingReconnect();
	VOID HandleReconnectSwitch();
	VOID HandleOutgoingSwitch();
	VOID HandleIncomingSwitch();
}