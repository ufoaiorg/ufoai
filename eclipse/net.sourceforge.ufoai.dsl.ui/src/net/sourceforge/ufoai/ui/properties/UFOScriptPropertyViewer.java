package net.sourceforge.ufoai.ui.properties;

import org.eclipse.swt.widgets.Composite;
import org.eclipse.xtext.ui.editor.XtextEditor;

public class UFOScriptPropertyViewer extends XtextEditor {

	private UFOScriptSelectionChangeListener listener;

	@Override
	public void createPartControl(Composite parent) {
		super.createPartControl(parent);
		listener = new UFOScriptSelectionChangeListener(this);
		listener.install(getSelectionProvider());
	}

	@Override
	public void dispose() {
		listener.uninstall(getSelectionProvider());
		super.dispose();
	}
}
