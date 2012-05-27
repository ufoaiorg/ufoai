package net.sourceforge.ufoai.editor;

import java.io.File;
import java.io.IOException;

import net.sourceforge.ufoai.preferences.Preferences;
import net.sourceforge.ufoai.preferences.PreferenceConstants;
import net.sourceforge.ufoai.util.StringUtil;
import net.sourceforge.ufoai.util.UIUtil;

import org.eclipse.core.runtime.IPath;
import org.eclipse.swt.SWT;
import org.eclipse.ui.IEditorLauncher;

public class MapEditor implements IEditorLauncher {
	@Override
	public void open(IPath file) {
		String mapfile = file.toOSString();
		String uforadiantBinary = Preferences.getString(PreferenceConstants.UFORADIANT_BINARY);
		if (StringUtil.isNull(uforadiantBinary)) {
			UIUtil.openMessageBox("No UFORadiant binary set", SWT.OK);
			return;
		}
		String execPath = Preferences.getString(PreferenceConstants.UFORADIANT_EXEC_PATH);
		if (StringUtil.isNull(uforadiantBinary)) {
			UIUtil.openMessageBox("No UFORadiant working directory set", SWT.OK);
			return;
		}

		// TODO: get full path to the map - open eclipse console with uforadiant
		// output
		ProcessBuilder p = new ProcessBuilder(uforadiantBinary, mapfile);
		p.directory(new File(execPath));
		try {
			Process process = p.start();
			// process.getErrorStream();
		} catch (IOException e) {
		}
	}
}
