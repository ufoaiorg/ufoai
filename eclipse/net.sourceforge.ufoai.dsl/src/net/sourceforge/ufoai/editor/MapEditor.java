package net.sourceforge.ufoai.editor;

import java.io.IOException;
import java.util.prefs.PreferenceChangeEvent;

import net.sourceforge.ufoai.preferences.PreferenceConstants;
import net.sourceforge.ufoai.preferences.Preferences;
import net.sourceforge.ufoai.util.StringUtil;
import net.sourceforge.ufoai.util.UIUtil;

import org.eclipse.core.runtime.IPath;
import org.eclipse.swt.SWT;
import org.eclipse.ui.IEditorLauncher;

public class MapEditor implements IEditorLauncher {
	@Override
	public void open(IPath file) {
		String mapfile = file.toOSString();
		String uforadiantBinary = Preferences.getString(PreferenceConstants.UFORADIANT_EXEC_PATH);
		if (StringUtil.isNull(uforadiantBinary)) {
			UIUtil.openMessageBox("No UFORadiant binary set", SWT.OK);
			return;
		}

		// TODO: get full path to the map - open eclipse console with uforadiant
		// output
		ProcessBuilder p = new ProcessBuilder(uforadiantBinary, mapfile);
		try {
			Process process = p.start();
			// process.getErrorStream();
		} catch (IOException e) {
		}
	}
}
