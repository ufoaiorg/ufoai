package net.sourceforge.ufoai.preferences;

import net.sourceforge.ufoai.UFOAIPlugin;

import org.eclipse.core.runtime.preferences.AbstractPreferenceInitializer;
import org.eclipse.jface.preference.IPreferenceStore;

public class Preferences extends AbstractPreferenceInitializer {
	@Override
	public void initializeDefaultPreferences() {
		IPreferenceStore store = getPreferences();
		store.setDefault(PreferenceConstants.UFORADIANT_EXEC_PATH, "");
		store.setDefault(PreferenceConstants.UFORADIANT_BINARY, "uforadiant");
	}

	public static IPreferenceStore getPreferences() {
		return UFOAIPlugin.getDefault().getPreferenceStore();
	}

	public static String getString(String prefName) {
		return getPreferences().getString(prefName);
	}

	public static void setString(String prefName, String newValue) {
		getPreferences().setValue(prefName, newValue);
	}
}
