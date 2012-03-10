package cgEditor.preferences;

import org.eclipse.core.runtime.preferences.AbstractPreferenceInitializer;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.preference.PreferenceConverter;
import org.eclipse.swt.graphics.RGB;

import cgEditor.CgEditorPlugin;

/**
 * Class used to initialize default preference values.
 */
public class PreferenceInitializer extends AbstractPreferenceInitializer {
	/*
	 * (non-Javadoc)
	 *
	 * @seeorg.eclipse.core.runtime.preferences.AbstractPreferenceInitializer#
	 * initializeDefaultPreferences()
	 */
	@Override
	public void initializeDefaultPreferences() {
		IPreferenceStore store = CgEditorPlugin.getDefault()
				.getPreferenceStore();
		// store.setDefault(PreferenceConstants.CGPROFILE,
		// PreferenceConstants.cgProfiles[0]);
		for (int i = 0; i < PreferenceConstants.PREFERENCES.length; i++) {
			PreferenceConverter.setDefault(store,
					PreferenceConstants.PREFERENCES[i][1],
					(RGB) PreferenceConstants.DEFAULTPREFERENCES[i][0]);
			store.setDefault(PreferenceConstants.PREFERENCES[i][2],
					(Integer) PreferenceConstants.DEFAULTPREFERENCES[i][1]);
		}
	}
}
