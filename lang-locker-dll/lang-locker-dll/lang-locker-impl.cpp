/*
 * lang-locker-impl.cpp : Implementation of control/locking of input language switches.
 */

#include "stdafx.h"
#include "lang-locker.h"
#include "msgnames.h"

// Determines the current locking state
HKL lockedLanguageHandle = 0;

// Set if unwanted language switch was not blocked, and thus shall be reverted at first possibility
bool revertLanguageRequired = false;

HHOOK messagesHook = 0;
HHOOK shellHook = 0;

// it is preferred that all checks and sets of the language are performed on 'main' thread, which gets widnows messages
// this ID is set at the first caught message; until that, value of '0' means 'current thread'
DWORD mainThreadId = 0;

// activates the specified language if it is avalable.
// returns whether activation succeeded
bool SetInputLanguage(HKL languageHandle) {
	HKL result = ActivateKeyboardLayout(languageHandle, KLF_ACTIVATE | KLF_SUBSTITUTE_OK | KLF_SETFORPROCESS);
	Log("SetInputLanguage() to ", languageHandle);

	return result != 0;
}

#ifdef _DEBUG
char* findMessageName(UINT code) {
	for (int i = 0; i < sizeof(WindowsMessages) / sizeof(WindowsMessage); i++) {
		if (WindowsMessages[i].msgid == code) {
			return WindowsMessages[i].pname;
		}
	}

	return "Unknown";
}
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

LRESULT WINAPI HookShellProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	// the shell hook works for two purposes:
	// 1) catches 'language change' events (HSHELL_LANGUAGE)
	// 2) ensures the correct language at the first activation of the window (e.g. if 
	//     Alt-Tab from another window with another language)
	if (!mainThreadId) {
		mainThreadId = GetCurrentThreadId();
		Log("HookShellProc sets mainThread=", GetCurrentThreadId());
	}
	
	switch (nCode) {
	case HSHELL_WINDOWACTIVATED:
	case HSHELL_LANGUAGE:
		if (!revertLanguageRequired && lockedLanguageHandle && GetKeyboardLayout(mainThreadId) != lockedLanguageHandle) {
			Log("HookShellProc: Detected a need to change the input language");
			revertLanguageRequired = true;
		}
	}
	if (revertLanguageRequired && lockedLanguageHandle && nCode == HSHELL_WINDOWACTIVATED) {
		// seems safe to change the language at this event
		revertLanguageRequired = false;
		Log("HookShellProc: change the input language to ", lockedLanguageHandle);
		SetInputLanguage(lockedLanguageHandle);
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT WINAPI HookGetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (!mainThreadId) {
		mainThreadId = GetCurrentThreadId();
		Log("HookGetMsgProc sets mainThread=", GetCurrentThreadId());
	}
	
	PMSG pmsg = (PMSG)lParam;

	// uncomment for more debug information, but only in DEBUG mode!
	// logfile << "HookGetMsgProc(): " << findMessageName(pmsg->message) << std::endl;

	if (nCode < 0)  // do not process message 
		return CallNextHookEx(NULL, nCode, wParam, lParam);

	switch (pmsg->message) {
	// before Windows XP, this message was sent before language switch, so it could be 
	// used for blocking switches. But, it does not work anymore, and left only as a 
	// reference
	case WM_INPUTLANGCHANGEREQUEST:
		if (!revertLanguageRequired) {
			Log("HookGetMsgProc: Input Language switch blocked in WM_INPUTLANGCHANGEREQUEST");
			pmsg->message = WM_NULL;
		}
		break;

	// list of messages which shall not be used for reverting languages
	case WM_DESTROY:
		break;

	case WM_INPUTLANGCHANGE:
		// NOTE: In Windows 8, this message may be not sent to hooks in some cases. So, other detectors (like "sink" or "shell hook")
		//       are required too

		// failed to block language change, check if need to be reverted in future
		if (!revertLanguageRequired && lockedLanguageHandle && (HKL)pmsg->lParam != lockedLanguageHandle) {
			revertLanguageRequired = true;
			Log("HookGetMsgProc(WM_INPUTLANGCHANGE): Detected a need to change the input language");
		}
		// NOTE: cannot change language here, as system will change it again (resulting in infintie loop)
		break;

	default:
		// other messages seems fine for language changes
		// if some message will be causing errors, they need to be added to list above
		if (revertLanguageRequired) {
			revertLanguageRequired = false;
			// logfile << "HookGetMsgProc(): " << "Attempt for change input language in msg " << findMessageName(pmsg->message) << std::endl;
			SetInputLanguage(lockedLanguageHandle);
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}


void SetWndHooksEnabled(bool enabled) {
	if (enabled) {
		if (!messagesHook) {
			messagesHook = SetWindowsHookEx(WH_GETMESSAGE, HookGetMsgProc, module, NULL);
			Log("Message hook set: ", messagesHook);
		}

		if (!shellHook) {
			shellHook = SetWindowsHookEx(WH_SHELL, HookShellProc, module, NULL);
			if (shellHook == NULL) {
				Log("Failed to set shell hook", GetLastError());
			}
			Log("Shell hook set: ", shellHook);
		}

	}
	else {
		if (messagesHook) {
			UnhookWindowsHookEx(messagesHook);
			messagesHook = NULL;
			Log("Message hook unset");
		}
		if (shellHook) {
			UnhookWindowsHookEx(shellHook);
			shellHook = NULL;
			Log("Shell hook unset");
		}
	}
}
