package com.excelsior.langlocker;

/**
 * Accessor to system-dependent native implementations of lock/unlock actions. 
 */
public class LockEngine {
	
	/**
	 * If the specified languageId is zero, then blocks input language switches
	 * and returns the current language handle. Otherwise, tries to switch to the
	 * specified language and, if succeeds, blocks further switches.
	 * 
	 * @param languageId the language ID returned by previous invocations of this method,
	 * 	or 0 to lock the current language
	 * 
	 * @return the ID of locked language, or 0 if failed to lock or switch to the specified language
	 */
	public static native long lockInputLanguage(long languageId);
	
	/**
	 * Unlocks the language, if previously locked.
	 */
	public static native void unlockInputLanguage();
	
	static {
		System.loadLibrary("lang-locker");
	}

}
