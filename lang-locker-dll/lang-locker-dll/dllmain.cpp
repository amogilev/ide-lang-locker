// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "lang-locker.h"


int nAttachedProcs = 0, nAttachedThreads = 0;
bool initialized = false;
HMODULE module;

void onAttach(bool isProc, HMODULE hModule) {
	if (!initialized) {
		initialized = true;
		module = hModule;
		Init();
	}
	if (isProc) {
		nAttachedProcs++;
		Log("PROCESS_ATTACH, threadId=", GetCurrentThreadId());
	} else {
		nAttachedThreads++;
//		Log("THREAD_ATTACH, threadId=", GetCurrentThreadId());
	}

}

void onDetach(bool isProc) {
	if (isProc) {
		nAttachedProcs--;
		Log("PROCESS_DETACH, threadId=", GetCurrentThreadId());
	} else {
//		Log("THREAD_DETACH, threadId=", GetCurrentThreadId());
		nAttachedThreads--;
	}
	if (nAttachedProcs <= 0 && nAttachedThreads <= 0) {
		Cleanup();
	}
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		onAttach(true, hModule);
		break;
	case DLL_THREAD_ATTACH:
		onAttach(false, hModule);
		break;
	case DLL_THREAD_DETACH:
		onDetach(false);
		break;
	case DLL_PROCESS_DETACH:
		onDetach(true);
		break;
	}
	return TRUE;
}

