package net.sourceforge.ufoai.preferences;

import net.sourceforge.ufoai.UFOAIPlugin;

import org.eclipse.jface.preference.DirectoryFieldEditor;
import org.eclipse.jface.preference.FieldEditor;
import org.eclipse.jface.preference.FieldEditorPreferencePage;
import org.eclipse.jface.preference.FileFieldEditor;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchPreferencePage;

public class UFOAIPreferencesPage extends FieldEditorPreferencePage implements IWorkbenchPreferencePage {
	public UFOAIPreferencesPage() {
		super(GRID);
		setPreferenceStore(UFOAIPlugin.getDefault().getPreferenceStore());
	}

	@Override
	public void init(IWorkbench workbench) {
	}

	@Override
	protected void createFieldEditors() {
		FieldEditor pathEditor = getDirectoryEditor(PreferenceConstants.UFORADIANT_EXEC_PATH, "UFORadiant path");
		FieldEditor binaryEditor = getFileEditor(PreferenceConstants.UFORADIANT_BINARY, "UFORadiant binary");
		addField(pathEditor);
		addField(binaryEditor);
	}

	private FileFieldEditor getFileEditor(String preferenceConstant, String title, boolean emptyAllowed) {
		FileFieldEditor editor = new FileFieldEditor(preferenceConstant, title, getFieldEditorParent());
		editor.setEmptyStringAllowed(emptyAllowed);
		return editor;
	}

	private DirectoryFieldEditor getDirectoryEditor(String preferenceConstant, String title, boolean emptyAllowed) {
		DirectoryFieldEditor editor = new DirectoryFieldEditor(preferenceConstant, title, getFieldEditorParent());
		editor.setEmptyStringAllowed(emptyAllowed);
		return editor;
	}

	private FileFieldEditor getFileEditor(String id, String title) {
		return getFileEditor(id, title, true);
	}

	private DirectoryFieldEditor getDirectoryEditor(String id, String title) {
		return getDirectoryEditor(id, title, true);
	}
}
