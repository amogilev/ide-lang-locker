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
	Log("Unknown Message: ", code);
	return "Unknown";
}
#endif

void Init() {
#ifdef _DEBUG
//	char buf[100];
//	sprintf_s(buf, "C:/temp/lang-locker-%d.log", GetCurrentProcessId());
//	logfile.open(buf);
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

BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM lParam) {
	*(BOOL*)lParam = TRUE;
	return FALSE;
}

BOOL HasWindow(DWORD threadId) {
	BOOL hasWnd = FALSE;
	EnumThreadWindows(threadId, &EnumThreadWndProc, (LPARAM)&hasWnd);
	return hasWnd;
}

DWORD FindSingleWindowThreadId()
{
	DWORD procId = GetCurrentProcessId();
	DWORD foundThreadId = NULL;
	HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
	THREADENTRY32 te32;
	te32.dwSize = sizeof(THREADENTRY32);

	hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE)
	{
		Log("CreateToolhelp32Snapshot failed");
		return NULL;
	}

	if (!Thread32First(hThreadSnap, &te32))
	{
		Log("Thread32First Failed");
		CloseHandle(hThreadSnap);
		return NULL;
	}

	// walk the thread list, for each thread of this process, check if it has any windows
	do
	{
		if (te32.th32OwnerProcessID == procId)
		{
			if (HasWindow(te32.th32ThreadID)) {
				Log("Thread with window(s): ", te32.th32ThreadID);
				if (foundThreadId) {
					Log("Multiple threads with windows found, give up...");
					return NULL;
				}
				else {
					foundThreadId = te32.th32ThreadID;
				}
			}
		}
	} while (Thread32Next(hThreadSnap, &te32));
	CloseHandle(hThreadSnap);

	return foundThreadId;
}

void DetectMainThread() {
	DWORD currentThreadId = GetCurrentThreadId();
	if (HasWindow(currentThreadId)) {
		Log("DetectMainThread() detected the current thread as the main thread: ", currentThreadId);
		mainThreadId = currentThreadId;
	}
	else {
		DWORD tid = FindSingleWindowThreadId();
		if (tid) {
			Log("DetectMainThread() detected the main thread: ", tid);
			mainThreadId = tid;
		}
		else {
			Log("DetectMainThread() failed to detect the main thread!");
		}
	}
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
	if (!mainThreadId) {
		DetectMainThread();
	}
	if (enabled) {
		if (!messagesHook) {
			messagesHook = SetWindowsHookEx(WH_GETMESSAGE, HookGetMsgProc, module, mainThreadId);
			Log("Message hook set for thread ", mainThreadId);
		}

		if (!shellHook) {
			shellHook = SetWindowsHookEx(WH_SHELL, HookShellProc, module, mainThreadId);
			if (shellHook == NULL) {
				Log("Failed to set shell hook", GetLastError());
			}
			Log("Shell hook set for thread ", mainThreadId);
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
