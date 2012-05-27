package net.sourceforge.ufoai.preferences;

import net.sourceforge.ufoai.UFOAIPlugin;

import org.eclipse.core.runtime.preferences.AbstractPreferenceInitializer;
import org.eclipse.jface.preference.IPreferenceStore;

public class PreferenceInitializer extends AbstractPreferenceInitializer {
	@Override
	public void initializeDefaultPreferences() {
		IPreferenceStore store = UFOAIPlugin.getDefault().getPreferenceStore();
		store.setDefault(PreferenceConstants.UFORADIANT_EXEC_PATH, "");
	}
}
