#pragma once
#include "framework.h"
#include "parser.h"
#include "timestr.h"

namespace Video {
	enum STATE: BYTE {
		IDLE, //just sitting
		WAIT, //waiting login
		RECORD, //recording a session
		PLAY, //playing a video
		SCROLL, //using the scrollbar to skip while playing
	};
	extern STATE State;

	struct Session;
	struct Packet: public PacketBase {
		static HANDLE NewHeap;
		DWORD Time;
		Packet *Next;
		Session *Login;

		LPVOID operator new(size_t Size, CONST PacketData* CONST Src) {
			return Parser->RecordPacket(Src);
		}
		VOID operator delete(LPVOID Ptr, CONST PacketData* CONST Src) {
			HeapFree(NewHeap, NULL, Ptr);
		}
		VOID operator delete(LPVOID Ptr);

		Packet(CONST Packet *CONST Previous, CONST WORD Delay): Time(Previous->Time + Delay), Next(NULL), Login(Previous->Login) {
			P = (PacketData*)(LPBYTE(this) + sizeof(Packet));
		}

		VOID EndSession();
		VOID CutSession();

		DWORD TimeInSession() CONST;
		BOOL IsLast() CONST;

	protected:
		Packet(): Time(0), Next(NULL), Login((Session *) this) { }
		Packet(CONST Packet *CONST Previous): Time(Previous->Time + 1000), Next(NULL), Login((Session *) this) { }
	};

	struct Session: public Packet {
		Packet *Prev;
		Packet *Last;
		DWORD PlayerID;
		HANDLE Heap;

		LPVOID operator new(size_t Size, CONST PacketData* CONST Src) {
			NewHeap = HeapCreate(NULL, NULL, NULL);
			if (!NewHeap) throw bad_alloc();
			return Parser->RecordSession(Src);
		}
		VOID operator delete(LPVOID Ptr, CONST PacketData* CONST Src) {
			HeapDestroy(NewHeap);
		}
		VOID operator delete(LPVOID Ptr) {
			HeapDestroy(((Session*)Ptr)->Heap);
		}

		Session(): Packet(), Prev(NULL), PlayerID(Parser->PlayerID), Heap(NewHeap) {
			P = (PacketData*)(LPBYTE(this) + sizeof(Session));
		}
		Session(Packet *CONST Previous): Packet(Previous), Prev(Previous), PlayerID(Parser->PlayerID), Heap(NewHeap) {
			P = (PacketData*)(LPBYTE(this) + sizeof(Session));
		}

		DWORD SessionTime() CONST {
			return Last->Time - Time;
		}
	};

	extern Packet *First;
	extern Packet *Last;
	extern Packet *Current;

	VOID HandleTibiaClosed();
	VOID OpenCmd(CONST LPCTSTR CmdLine);
	VOID OpenDrop(CONST HDROP Drop);
	VOID FileDialog();
	VOID SaveChanges();
	VOID SaveRecovery();
	VOID Close();
	VOID Delete();
	VOID Start();
	VOID Abort();
	VOID WaitClose();
	VOID WaitDelete();
	VOID Record();
	VOID RecordNext();
	VOID Cancel();
	VOID Continue();
	VOID Stop();
	VOID Play();
	VOID Eject();
	VOID Logout();
	VOID PlayClose();
	VOID PlayTimer();
	VOID PlayPause();
	VOID Resume();
	VOID Pause();
	VOID SlowDown();
	VOID SpeedUp();
	VOID SetSpeed(CONST INT NewSpeed);
	VOID SetLight(CONST BYTE Light);
	VOID PlayDelete();
	VOID SessionSelect();
	VOID SessionSelect(CONST INT Session);
	VOID SessionStart();
	VOID SessionNext();
	VOID SessionPrev();
	VOID SkipStart();
	VOID SkipBackward(CONST DWORD Time);
	VOID SkipEnd();
	VOID SkipForward(CONST DWORD Time);
	VOID SkipPosition(CONST DWORD Time);
	VOID SkipScroll(CONST WORD Code);
	VOID ScrollRepeat(CONST WORD Code);
	VOID ScrollIdle();
	VOID ScrollSessionSelect();
	VOID ScrollDelete();
	VOID PlayFirstPacket();
	VOID PlayLastPacket();
	VOID CutStart();
	VOID CutEnd();
	VOID AddFast(DWORD TimeBias);
	VOID AddSlow(DWORD TimeBias);
	VOID AddSkip(DWORD TimeBias);
	VOID AddDelay(WORD TimeBias);
	VOID AddLight(CONST BYTE Light);
}