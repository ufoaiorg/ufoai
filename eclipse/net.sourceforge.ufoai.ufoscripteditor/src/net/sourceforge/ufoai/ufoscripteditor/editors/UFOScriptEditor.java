package net.sourceforge.ufoai.ufoscripteditor.editors;

import net.sourceforge.ufoai.ufoscripteditor.views.outline.UFOOutlinePage;

import org.eclipse.ui.editors.text.TextEditor;
import org.eclipse.ui.views.contentoutline.IContentOutlinePage;

public class UFOScriptEditor extends TextEditor {
	public static String ID = "net.sourceforge.ufoai.ufoscripteditor.editors.ufoscripteditor";
	private final ColorManager colorManager;
	private UFOOutlinePage outlinePage;

	public UFOScriptEditor() {
		super();
		colorManager = new ColorManager();
		setSourceViewerConfiguration(new UFOConfiguration(colorManager));
		setDocumentProvider(new UFODocumentProvider());
	}

	@Override
	public void dispose() {
		colorManager.dispose();
		super.dispose();
	}

	@Override
	public Object getAdapter(Class required) {
		if (IContentOutlinePage.class.equals(required)) {
			if (outlinePage == null) {
				outlinePage = new UFOOutlinePage(getDocumentProvider(), this);
				outlinePage.setInput(getEditorInput());
			}
			return outlinePage;
		}
		return super.getAdapter(required);
	}
}
