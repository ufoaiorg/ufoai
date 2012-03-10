package cgEditor;

import org.eclipse.jface.preference.PreferenceConverter;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.rules.Token;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

import cgEditor.editors.TokenManager;
import cgEditor.preferences.PreferenceConstants;

/**
 * The main plugin class to be used in the desktop.
 */
public class CgEditorPlugin extends AbstractUIPlugin {
	// The shared instance.
	private static CgEditorPlugin plugin;

	/**
	 * The constructor.
	 */
	public CgEditorPlugin() {
		plugin = this;
	}

	/**
	 * This method is called upon plug-in activation
	 */
	@Override
	public void start(BundleContext context) throws Exception {
		super.start(context);

		for (int i = 0; i < PreferenceConstants.PREFERENCES.length; i++) {
			Token token = TokenManager
					.getToken(PreferenceConstants.PREFERENCES[i][0]);
			RGB rgb = PreferenceConverter.getColor(getPreferenceStore(),
					PreferenceConstants.PREFERENCES[i][1]);
			int style = getPreferenceStore().getInt(
					PreferenceConstants.PREFERENCES[i][2]);
			token.setData(new TextAttribute(TokenManager.getColor(rgb), null,
					style));
		}
	}

	/**
	 * This method is called when the plug-in is stopped
	 */
	@Override
	public void stop(BundleContext context) throws Exception {
		super.stop(context);
		plugin = null;
	}

	/**
	 * Returns the shared instance.
	 */
	public static CgEditorPlugin getDefault() {
		return plugin;
	}

	/**
	 * Returns an image descriptor for the image file at the given plug-in
	 * relative path.
	 *
	 * @param path
	 *            the path
	 * @return the image descriptor
	 */
	public static ImageDescriptor getImageDescriptor(String path) {
		return AbstractUIPlugin.imageDescriptorFromPlugin("cgEditor", path);
	}
}
