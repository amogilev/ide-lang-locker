#include "stdafx.h"
#include "lang-locker.h"

extern "C" {
JNIEXPORT jlong JNICALL Java_com_gilecode_langlocker_LockEngine_lockInputLanguage(JNIEnv * env, jclass clazz, jlong language) {
	return (jlong)LockInputLanguage((HKL)language);
}

JNIEXPORT void JNICALL Java_com_gilecode_langlocker_LockEngine_unlockInputLanguage(JNIEnv * env, jclass clazz) {
	UnlockInputLanguage();
}
}	