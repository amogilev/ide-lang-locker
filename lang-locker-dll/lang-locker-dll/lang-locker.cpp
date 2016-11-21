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

LANGLOCKERDLL_API HKL LockInputLanguage(HKL langHandle) {
	HKL curLang = GetKeyboardLayout(mainThreadId);
	Log("LockInputLanguage(), curLang=", curLang);
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
		SetWndHooksEnabled(true);

		Log("Input language locked to ", lockedLanguageHandle);

		// activate the language again to protect from the potential concurrency issues
		SetInputLanguage(langHandle);

		// NOTE: if the windows is not active, SetInputLanguge() is actually ignored by Windows, with a fake "success" result.
		// So, we cannot rely only on the code above, but also needs a way to catch WM_ACTIVATE and check the current language there

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
		SetWndHooksEnabled(false);
		lockedLanguageHandle = NULL;
		Log("Input language unlocked");
	}
}
