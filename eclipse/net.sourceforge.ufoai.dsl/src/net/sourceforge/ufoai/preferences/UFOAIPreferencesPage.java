package net.sourceforge.ufoai.preferences;

import net.sourceforge.ufoai.UFOAIPlugin;

import org.eclipse.jface.preference.FieldEditorPreferencePage;
import org.eclipse.jface.preference.FileFieldEditor;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchPreferencePage;

public class UFOAIPreferencesPage extends FieldEditorPreferencePage implements IWorkbenchPreferencePage {
	public UFOAIPreferencesPage() {
		super(GRID);
		setPreferenceStore(UFOAIPlugin.getDefault().getPreferenceStore());
		setDescription("UFOAI preferences");
	}

	@Override
	public void init(IWorkbench workbench) {
	}

	@Override
	protected void createFieldEditors() {
		getFileEditor(PreferenceConstants.UFORADIANT_EXEC_PATH, "UFORadiant binary");
	}

	private FileFieldEditor getFileEditor(String preferenceConstant, String title, boolean emptyAllowed) {
		FileFieldEditor editor = new FileFieldEditor(preferenceConstant, title, getFieldEditorParent());
		editor.setEmptyStringAllowed(emptyAllowed);
		return editor;
	}

	private FileFieldEditor getFileEditor(String id, String title) {
		return getFileEditor(id, title, true);
	}
}
