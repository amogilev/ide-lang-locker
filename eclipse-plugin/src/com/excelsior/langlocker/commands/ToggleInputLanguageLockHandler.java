package com.excelsior.langlocker.commands;

import org.eclipse.core.commands.Command;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.IHandler;
import org.eclipse.core.commands.IHandlerListener;
import org.eclipse.core.commands.State;
import org.eclipse.core.runtime.preferences.IEclipsePreferences;
import org.eclipse.core.runtime.preferences.InstanceScope;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.commands.ICommandService;
import org.eclipse.ui.handlers.RegistryToggleState;

import com.excelsior.langlocker.LockEngine;

/**
 * The handler which processes clicks on "Lock/unlock input language" button.
 */
public class ToggleInputLanguageLockHandler implements IHandler {
	
	private static final String PREF_NODE = "com.excelsior.langlocker";
	private static final String PREFID_LANGUAGE = "lockedLanguageId";
	
	/**
	 * The constructor is invoked when the plugin starts, and restores the
	 * persisted lock state.
	 */
	public ToggleInputLanguageLockHandler() {
		restoreLockState();
	}

	/**
	 * Restores the persisted lock state (whether is locked) and, if
	 * was locked, the previously locked language.
	 */
	private void restoreLockState() {
		ICommandService commandService = (ICommandService) PlatformUI
				.getWorkbench().getActiveWorkbenchWindow()
				.getService(ICommandService.class);
		Command command = commandService
				.getCommand("com.excelsior.langlocker.commands.toggleLanguageLock");
		if (command == null) {
			return;
		}
		
		// apply the lock according to the current state
		updateLockStates(command, false);
	}
	
	/**
	 * Tries to apply the locked state of the command, either directly (i.e. "restore state"),
	 * or after changing it to the opposite one (i.e. "toggle state").
	 * <p/>
	 * Takes into the account and updates "locked language id" stored in the preferences.
	 * If locking failed, reverts the command's state to "unlocked".
	 * 
	 * @param command the command which owns the lock state
	 * @param toggle whether to change the current locked state to opposite, or just restore it.
	 */
	private void updateLockStates(Command command, boolean toggle) {
		State state = command.getState(RegistryToggleState.STATE_ID);
		if(state == null) {
			// unexpected
			throw new IllegalStateException("The command's toggle state is missing");
		}
		if(!(state.getValue() instanceof Boolean)) {
			// unexpected
			throw new IllegalStateException("The command's toggle state doesn't contain a boolean value");
		}
		
		// check if locked
		boolean prevLocked, newLocked; // previous and new command states
		prevLocked = ((Boolean) state.getValue()).booleanValue();
		newLocked = toggle ? !prevLocked : prevLocked;
		
		// locked language is stored in preferences. It is used only in 'restore' action, as while
		// toggling we always locks to the current language
		IEclipsePreferences preferences = getPreferences();
		long langId = toggle ? 0 : preferences.getLong(PREFID_LANGUAGE, 0);
		
		// try apply the new state
		if (!newLocked) {
			LockEngine.unlockInputLanguage();
			langId = 0;
		} else {
			// lock either current or specified language, depending on whether langId is 0
			langId = LockEngine.lockInputLanguage(langId);
			if (langId == 0) {
				// failed to lock, revert the command/button state to "unlocked"
				newLocked = false;
			}
		}
		
		// persist the new states
		preferences.putLong(PREFID_LANGUAGE, langId);
		state.setValue(Boolean.valueOf(newLocked));
	}

	/**
	 * Obtains and returns the persisted preferences for the plugin.
	 */
	@SuppressWarnings("deprecation")
	private IEclipsePreferences getPreferences() {
		// NOTE: the deprecated "new InstanceScope()" is used here instead of 
		//       "InstanceScope.INSTANCE" to provide compatibility with 
		//       older Eclipse versions 
		return new InstanceScope().getNode(PREF_NODE);
	}
	
	@Override
	public void addHandlerListener(IHandlerListener handlerListener) {
	}

	@Override
	public void dispose() {
	}

	@Override
	public Object execute(ExecutionEvent event) throws ExecutionException {
		Command command = event.getCommand();
		
		// toggle and apply locked state
		updateLockStates(command, true);
		
		return null;
	}

	@Override
	public boolean isEnabled() {
		return true;
	}

	@Override
	public boolean isHandled() {
		return true;
	}

	@Override
	public void removeHandlerListener(IHandlerListener handlerListener) {
	}

}
