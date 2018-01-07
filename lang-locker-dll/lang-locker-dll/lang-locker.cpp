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

void Log(const wchar_t* str) {
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
void Log(const wchar_t* str) {}
void Log(const char* str, HANDLE param) {}
void Log(const char* str, DWORD param) {}
#endif

LANGLOCKERDLL_API HKL LockInputLanguage(HKL langHandle) {
	if (!uiThreadId) {
		uiThreadId = GetCurrentThreadId();
		Log("UI thread detected: ", uiThreadId);
	}
	if (!mainThreadId) {
		DetectMainThread();
	}

	HKL allLangs[10];
	int langsCount = GetKeyboardLayoutList(10, allLangs);
	for (int i = 0; i < langsCount; i++) {
		logfile << "Language handle 0x" << std::hex << std::uppercase << allLangs[i] << std::endl;
	}

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

		if (langHandle == curLang && mainThreadId != uiThreadId) {
			// already was at the needed language. Unfortunately, in IDEA & Win10 it causes the problem - the next manual switch is reported 
			//  to neither MsgProc nor ShellProc, so it is not detected, not prevented and not reverted. Thus it causes the following bug:
			//  - get a system with two languages installed (e.g. English and Russian)
			//  - unlock language (or start IDEA with no lock)
			//  - switch language
			//  - lock language
			//  - then try to switch languages again several times. In most cases, the system allows one switch to "incorrect" language and 
			//     one switch back to "correct" one, and only then the "correct" language is actually "locked".

			//  There is no such problem for Eclipse, probably because its UI thread (EventQueue) is the same as main window thread, i.e. "mainThreadId == uiThreadId"
			//  For IDEA, these threads are different, the manual switches changes language (as per GetKeyboardLayout(threadId)) for the UI thread but not 
			//  for the main thread, so Windows (under some additional conditions that are not quite clear yet) "skips" these events to hooks (which are in main 
			//  thread) as if its langauge is not changed .

			// I have found two possible solutions:
			// 1) (BAD!) Set hooks from the very start of the DLL and keep them until unloaded; that helps for all locks except the first one.
			// 2) (GOOD! paradoxical but working!) During a lock from UI thread, set the INCORRECT language and rely on hooks to change it to the correct one.
			// The solution (2) is implemented below

			// pre-switch to another language to avoid missing HSHELL_LANGUAGE on further switches
			HKL otherLang = 0;
			for (int i = 0; i < langsCount; i++) {
				if (langHandle != allLangs[i]) {
					otherLang = allLangs[i];
					break;
				}
			}
			if (otherLang) {
				Log("Temporary switch to another language! ", otherLang);
				ActivateKeyboardLayout(otherLang, KLF_ACTIVATE | KLF_SUBSTITUTE_OK | KLF_SETFORPROCESS);
				revertLanguageRequired = true; // not actually required, but may be useful for faster switching to the correct language
			}
		}

		Log("Input language locked to ", lockedLanguageHandle);

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
