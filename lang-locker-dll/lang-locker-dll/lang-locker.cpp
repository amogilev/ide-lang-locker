/*
 lang-locker-dll.cpp : Defines the exported functions for the DLL application.
*/

#include "stdafx.h"
#include "lang-locker.h"

using namespace std;

#ifdef _DEBUG

std::ofstream logfile;

void Log(const char* str) {
	logfile << str << endl;
}

void Log(const char* str, HANDLE param) {
	logfile << str << "0x" << hex << uppercase << param << endl;
}

void Log(const char* str, DWORD param) {
	logfile << str << "0x" << hex << uppercase << param << endl;
}


#else 
void Log(const char* str) {}
void Log(const char* str, HANDLE param) {}
void Log(const char* str, DWORD param) {}
#endif

void Init() {
#ifdef _DEBUG
	logfile.open("lang-locker.log");
#ifdef MYWIN64
	Log("lang-locker.dll 64-bit initialized");
#else
	Log("lang-locker.dll 32-bit initialized");
#endif
#endif
}

void Cleanup() {
	UnlockInputLanguage();
#ifdef _DEBUG
	Log("lang-locker.dll detached, cleanup");
	logfile.close();
#endif
}


LANGLOCKERDLL_API HKL LockInputLanguage(HKL langHandle) {
	HKL curLang = GetKeyboardLayout(0);
	bool success = false;

	if (!lockedLanguageHandle) {
		if (!langHandle) {
			langHandle = curLang;
		}

		if (langHandle != curLang) {
			// try to switch to the specified language
			if (!SetInputLanguage(langHandle)) {
				Log("Failed to switch to the requested language ", langHandle);
				return 0;
			}
		}

		lockedLanguageHandle = langHandle;
		SetWndMessageHookEnabled(true);
		SetLockingSinkEnabled(true);

		Log("Input language locked to ", lockedLanguageHandle);

		// activate the language again to protect from the potential concurrency issues
		SetInputLanguage(langHandle);

	} else if (langHandle != 0 && langHandle != curLang) {
		Log("Requested change of locked language to ", langHandle);

		// already locked, but for another language. Try to change it
		HKL prevLockedLang = lockedLanguageHandle;
		lockedLanguageHandle = langHandle;

		if (!SetInputLanguage(langHandle)) {
			Log("Failed to switch to the requested language ", langHandle);

			// revert
			lockedLanguageHandle = prevLockedLang;
			SetInputLanguage(prevLockedLang);

			return prevLockedLang;
		}
	}

	return langHandle;
}

LANGLOCKERDLL_API void UnlockInputLanguage() {
	if (lockedLanguageHandle) {
		SetWndMessageHookEnabled(false);
		SetLockingSinkEnabled(false);
		lockedLanguageHandle = NULL;
		Log("Input language unlocked");
	}
}
