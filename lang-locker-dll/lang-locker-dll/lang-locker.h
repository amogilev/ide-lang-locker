/*
 * lang-locker.h : declaration of exported lock/unlock functions and common internally used stuff.
 */

#ifdef LANGLOCKERDLL_EXPORTS
#define LANGLOCKERDLL_API extern "C" __declspec(dllexport)
#else
#define LANGLOCKERDLL_API extern "C" __declspec(dllimport)
#endif

//
// The pair of exported functions below is intended for non-Java programs.
// Java programs shall use JNI versions declared in com_gilecode_langlocker_LockEngine.h
//

/*
 * If passed languageHandle is zero, then blocks input language switches
 * and returns the current language handle. Otherwise, tries to switch to the
 * specified language and, if succeeds, blocks further switches.

 * Returns the handle of the current locked input language, or 0 if failed to lock
 * to any language.
 */
LANGLOCKERDLL_API HKL LockInputLanguage(HKL languageHandle);

/*
 * Unblocks previously blocked input language switches.
 */
LANGLOCKERDLL_API void UnlockInputLanguage();

//
// Functions which shall be invoked at start and end of DLL lifecycle.
// 
void Init();
void Cleanup();

//
// Debug logging used throughout the lang-locker DLL code.
// In 'Debug' configuration, the logs are written into langlocker.log file
// created in the current application working directory. In 'Release' - does 
// nothing.
// 
void Log(const char* str);
void Log(const char* str, HANDLE param);
void Log(const char* str, DWORD param);
void Log(const wchar_t* str);

#ifdef _DEBUG
extern std::ofstream logfile;
#endif

extern HKL lockedLanguageHandle;
extern HMODULE module;
extern DWORD mainThreadId;
extern DWORD uiThreadId;
extern bool revertLanguageRequired;

// 
// Implementation methods which enables or disables messages hooks 
// which provide control over input languages switches
//
void SetWndHooksEnabled(bool enabled);

// Internal function used to switch current input language
bool SetInputLanguage(HKL languageHandle);

// Internal function used to detect the "main" thread, i.e. the thread with the window message loop
void DetectMainThread();
