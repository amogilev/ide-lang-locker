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
DWORD lockingSinkCookie;
bool lockingSinkSet = false;

bool coInitialized = false;
HRESULT coInitResult;

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

	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	Log("On ProcessAttach CoInitialized() with hr=", hr);
}

void Cleanup() {
	UnlockInputLanguage();
	if (coInitialized && coInitResult == S_OK) {
		CoUninitialize();
	}
#ifdef _DEBUG
	Log("lang-locker.dll detached, cleanup");
	logfile.close();
#endif
}

LRESULT WINAPI HookShellProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	// this check is used to ensure the correct language at the first activation of the window (e.g. if Alt-Tab from another 
	//  window with another language), for which Windows does not notify sink about the language change
	if (nCode == HSHELL_WINDOWACTIVATED) {
		if (!revertLanguageRequired && lockedLanguageHandle && GetKeyboardLayout(0) != lockedLanguageHandle) {
			Log("HookShellProc(HSHELL_WINDOWACTIVATED): Detected a need to change the input language");
			revertLanguageRequired = false;
			SetInputLanguage(lockedLanguageHandle);
		}
	}
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}


LRESULT WINAPI HookGetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
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
		// NOTE: In Windows 8, this message may be not sent to hooks in some cases. So, both "sink" and "hook"
		//       detectors are required

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


void SetWndMessageHookEnabled(bool enabled) {
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

/*
 * Notify sink which is able to monitor language switches and blocks switches 
 * in Windows 7 and below.
 */
class LockingLangNotifySink : public ITfLanguageProfileNotifySink
{
private:
	LONG _cRef;     // COM ref count
public:
	virtual HRESULT STDMETHODCALLTYPE OnLanguageChange(
		/* [in] */ LANGID langid,
		/* [out] */ __RPC__out BOOL *pfAccept)
	{
		// block language switch (does not work in Windows 8 or later)
		*pfAccept = 0;
		Log("LockingLangNotifySink: Input Language switch blocked");

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnLanguageChanged(void)
	{
		Log("LockingLangNotifySink: OnLanguageChanged()");

		// Since Windows 8, the OnLanguageChange method is never invoked
		// So, here we shall check the current language and later revert it if necessary
		if (!revertLanguageRequired && lockedLanguageHandle) {
			HKL curLang = GetKeyboardLayout(0);
			if (lockedLanguageHandle != curLang) {
				revertLanguageRequired = true;
				Log("LockingLangNotifySink: Detected a need to change the input language; curLang=", curLang);
				// cannot change language here, as system will change it again (resulting in infintie loop)
			}
		}

		return S_OK;
	}

	LockingLangNotifySink() {
		_cRef = 1;
	}
	~LockingLangNotifySink() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj) {
		if (ppvObj == NULL)
			return E_INVALIDARG;

		*ppvObj = NULL;

		if (IsEqualIID(riid, IID_IUnknown) ||
			IsEqualIID(riid, IID_ITfLanguageProfileNotifySink))
		{
			*ppvObj = (ITfLanguageProfileNotifySink *)this;
		}

		if (*ppvObj)
		{
			AddRef();
			return S_OK;
		}

		return E_NOINTERFACE;
	}

	STDMETHODIMP_(ULONG) AddRef(void) {
		return ++_cRef;
	}

	STDMETHODIMP_(ULONG) Release(void) {
		LONG cr = --_cRef;

		if (_cRef == 0)
		{
			delete this;
		}

		return cr;
	}


} m_LockingLangNotifySink;

void SetLockingSinkEnabled(bool enabled) {
	if (enabled == lockingSinkSet) {
		Log("SetLockingSinkEnabled: nothing to do");
		return;
	}

	HRESULT hr;

	if (!coInitialized) {
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
			Log("CoInitialized() succeeds with hr=", hr);

			coInitialized = true;
			coInitResult = hr;
		}
		else {
			Log("Failed to CoInitializeEx(), hr=", hr);
			return;
		}
	}

	ITfInputProcessorProfiles *pProfiles;

	//Create the object and obtain the pointer. 
	hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_ITfInputProcessorProfiles,
		(LPVOID*)&pProfiles);

	if (SUCCEEDED(hr))
	{
		ITfSource *pSource;

		hr = pProfiles->QueryInterface(IID_ITfSource, (LPVOID*)&pSource);
		if (SUCCEEDED(hr))
		{
			if (enabled) {
				hr = pSource->AdviseSink(IID_ITfLanguageProfileNotifySink,
					(ITfLanguageProfileNotifySink*)&m_LockingLangNotifySink,
					&lockingSinkCookie);
				if (SUCCEEDED(hr)) {
					Log("LockingSink advised successfully, cookie=", lockingSinkCookie);
					lockingSinkSet = true;
				}
				else {
					Log("Failed to advise LockingSink, hr=", hr);
				}
			}
			else {
				hr = pSource->UnadviseSink(lockingSinkCookie);
				if (SUCCEEDED(hr)) {
					Log("LockingSink unadvised successfully");
					lockingSinkSet = false;
				}
				else {
					Log("Failed to unadvise LockingSink, hr=", hr);
				}
			}

			pSource->Release();
		}
		else {
			Log("Failed to QueryInterface(IID_ITfSource), hr=", hr);
		}

		pProfiles->Release();
	}
	else {
		Log("Failed to CoCreateInstance(CLSID_TF_InputProcessorProfiles), hr=", hr);
	}
}
