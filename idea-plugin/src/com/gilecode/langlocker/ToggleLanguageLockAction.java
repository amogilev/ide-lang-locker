package com.gilecode.langlocker;

import com.intellij.ide.util.PropertiesComponent;
import com.intellij.openapi.actionSystem.AnAction;
import com.intellij.openapi.actionSystem.AnActionEvent;
import com.intellij.openapi.actionSystem.ToggleAction;

/**
 * @author Andrey Mogilev
 */
public class ToggleLanguageLockAction extends ToggleAction {

    private static final String PREF_LANGUAGE = "com.gilecode.langlocker.lockedLanguageId";

    private static boolean isLocked = false;

    public ToggleLanguageLockAction() {
        toggleOrRestoreLanguageLock(false);
    }

    /**
     * Tries to apply the lock state, either by a toggle action, or by restoring it at IDEA startup.
     * <p/>
     * Takes into the account and updates "locked language id" stored in the preferences.
     *
     * @param toggle whether to change the current locked state to the opposite, or just restore it
     *               to the persisted value.
     */
    private void toggleOrRestoreLanguageLock(boolean toggle) {
        // locked language is stored in preferences. It is used only in 'restore' action, as while
        // toggling we always locks to the current language
        PropertiesComponent props = PropertiesComponent.getInstance();

        long langId = toggle ? 0 : Long.valueOf(props.getValue(PREF_LANGUAGE, "0"));

//        System.out.println("BEGIN toggleOrRestoreLanguageLock(): toggle=" + toggle + "; isLocked=" + isLocked  +
//                "; langId=" + langId  +  "; prop = " + props.getValue(PREF_LANGUAGE));

        // check if locked
        boolean prevLocked, newLocked; // previous and new command states
        prevLocked = isLocked;
        newLocked = toggle ? !prevLocked : langId > 0;

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
        props.setValue(PREF_LANGUAGE, Long.toString(langId));
        isLocked = newLocked;

//        System.out.println("  END toggleOrRestoreLanguageLock(): toggle=" + toggle + "; isLocked=" + isLocked  +
//                "; langId=" + langId  +  "; prop = " + props.getValue(PREF_LANGUAGE));
    }

    @Override
    public boolean isSelected(AnActionEvent anActionEvent) {
        return isLocked;
    }

    @Override
    public void setSelected(AnActionEvent anActionEvent, boolean b) {
        toggleOrRestoreLanguageLock(true);
    }
}
