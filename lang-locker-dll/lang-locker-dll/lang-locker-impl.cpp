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
DWORD lockingSinkCookie;
bool lockingSinkSet = false;

// activates the specified language if it is avalable.
// returns whether activation succeeded
bool SetInputLanguage(HKL languageHandle) {
	HKL result = ActivateKeyboardLayout(languageHandle, KLF_ACTIVATE | KLF_SUBSTITUTE_OK | KLF_SETFORPROCESS);
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


LRESULT WINAPI HookGetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	PMSG pmsg = (PMSG)lParam;

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

	// list of messages which shall not be used for reverting langauges
	case WM_DESTROY:
		break;

	case WM_INPUTLANGCHANGE:
		// NOTE: In Windows 8, this message may be not sent to hooks. So, both "sink" and "hook"
		//       detectors are required

		// failed to block language change, check if need to be reverted in future
		if (!revertLanguageRequired && lockedLanguageHandle && (HKL)pmsg->lParam != lockedLanguageHandle) {
			revertLanguageRequired = true;
			Log("HookGetMsgProc: Detected need to revert unbloked language switch");
		}
		// cannot change language here, as system will change it again (resulting in infintie loop)
		break;

	default:
		// other messages seems fine for language change
		// if some message will be causing errors, they need to be added to list above
		if (revertLanguageRequired) {
			revertLanguageRequired = false;
			Log("Attempt for revert unblocked input language switch in msg ", pmsg->message);
			SetInputLanguage(lockedLanguageHandle);
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}


void SetWndMessageHookEnabled(bool enabled) {
	if (enabled) {
		if (!messagesHook) {
			messagesHook = SetWindowsHookEx(WH_GETMESSAGE, HookGetMsgProc, NULL, GetCurrentThreadId());
			Log("Message hook set: ", messagesHook);
		}
	}
	else if (messagesHook) {
		UnhookWindowsHookEx(messagesHook);
		messagesHook = NULL;
		Log("Message hook unset");
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
		// block language switch
		*pfAccept = 0;
		Log("LockingLangNotifySink: Input Language switch blocked");

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnLanguageChanged(void)
	{
		// Since Windows 8, the OnLanguageChange method is never invoked
		// So, here we shall check the current language and later revert it if necessary
		if (!revertLanguageRequired && lockedLanguageHandle) {
			HKL curLang = GetKeyboardLayout(0);
			if (lockedLanguageHandle != curLang) {
				revertLanguageRequired = true;
				Log("LockingLangNotifySink: Detected need to revert unbloked language switch, curLang=", curLang);
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
