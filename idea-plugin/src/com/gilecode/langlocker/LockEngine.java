package com.gilecode.langlocker;

import com.intellij.openapi.diagnostic.Logger;
import com.intellij.openapi.util.io.FileUtil;
import com.intellij.util.io.IOUtil;

import java.io.*;
import java.net.URL;

/**
 * Accessor to system-dependent native implementations of lock/unlock actions.
 *
 * @author Andrey Mogilev
 */
public class LockEngine {

    private static final Logger log = Logger.getInstance(LockEngine.class);
    private static final String LOCKER_DLL = "lang-locker.dll";

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
        // TODO: check if throws are logged properly
        String osName = System.getProperty("os.name");
        if (osName == null || !osName.toLowerCase().startsWith("windows")) {
            throw new IllegalStateException("This plug-in works only for Windows, but 'os.name'=" + osName);
        }

        String arch = System.getProperty("sun.arch.data.model");
        String libPath;
        if ("64".equals(arch)) {
            libPath = "libs/x64/";
        } else if ("32".equals(arch)){
            libPath = "libs/win32/";
        } else {
            throw new IllegalStateException("Unexpected value for 'sun.arch.data.model':" + arch);
        }

        URL libUrl = LockEngine.class.getClassLoader().getResource(libPath + LOCKER_DLL);

        if (libUrl == null) {
            log.error("Could not find " + LOCKER_DLL);
        } else if (!new File(libUrl.getFile()).isFile()) {
            // probably, we are inside JAR. Copy DLL to a temporary file
            // TODO: maybe re-use existing DLL files? Or delete at exit...
            try {
                File tmpDll = FileUtil.createTempFile("tempLangLocker", ".dll");
                try(InputStream is = LockEngine.class.getClassLoader().getResourceAsStream(libPath + LOCKER_DLL);
                    OutputStream out = new FileOutputStream(tmpDll)
                ) {
                    FileUtil.copy(is, out);
                }
                tmpDll.deleteOnExit();

                // load native methods from the temporary DLL
                System.load(tmpDll.getAbsolutePath());
            } catch (IOException e) {
                log.error("Failed to create a temporary file", e);
            }
        } else {
            // load native methods from the plugin-hosted DLL
            System.load(libUrl.getFile());
        }
    }
}
