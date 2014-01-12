ide-lang-locker
===============

IDE plugin(s) for blocking unwanted input language switches.
Currently includes only Eclipse plugin for Windows. 
Supports 32-bit and 64-bit versions of Windows XP and later, including Windows 8.1.


===============
Build instructions
===============

I. At first, lang-locker.dll needs to be built. Do the following actions:
- Download and install "Microsoft Visual Studio Express 2013 for Windows Desktop" if needed;
- Download and install JDK if needed;
- Set %JAVA_HOME% environment vraiable to point the JDK installation folder;
- Run Visual Studio and load lang-locker-dll solution;
- Select preferred configuration (Debug or Release) and optionally modify build options (e.g.
  you may prefer to use standalone Runtime DLLs to make lang-locker.dll smaller - this is fine
  on developer computer but may not work on others without additional installation of vcredist);
- Build the DLL for both Win32 and x64 platforms;
- Copy the resulting DLLs from lang-locker-dll\Win32\${Configuration}\ to eclipse-plugin\libs\win32\
  and from lang-locker-dll\x64\${Configuration}\ to eclipse-plugin\libs\x64\

II. Build Eclipse plugin.
- Download and install Eclipse with PDE if you don't have one;
- Open eclipse-plugin project;
- Build and export plugin.


===============
How to use Eclipse plugin
===============

Add the resulting plugin JAR to 'dropins' folder of the target Eclipse installation, and restart
Eclipse. Then, look for a new button in top panel, which looks like a lock with 'EN' chracters on 
it. The button's hint shall be "Lock/unlock input language". Pressing the button will block the
input language switches while IDE window is active. In order to intentionally change the input
language, it shall be "unlocked" first, which can be done by clicking on the button again.

===============
Reporting errors
===============

In case of errors, please do the following:
- Use "debug" version of the DLLs and plugin;
- Delete logs from "Error Log" Eclipse view and restart Eclipse;
- Make sure the error persists;
- Find "lang-locker.log" in the Eclipse installation folder;
- Report the Windows version, Eclipse version, attach lang-locker.log and contents of Error Log view.

However, no support guarantees are provided.

