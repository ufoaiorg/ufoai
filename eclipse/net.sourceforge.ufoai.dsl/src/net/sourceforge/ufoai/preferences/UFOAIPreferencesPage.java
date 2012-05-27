package net.sourceforge.ufoai.preferences;

import java.io.File;

import net.sourceforge.ufoai.UFOAIPlugin;

import org.eclipse.jface.preference.DirectoryFieldEditor;
import org.eclipse.jface.preference.FieldEditorPreferencePage;
import org.eclipse.jface.preference.FileFieldEditor;
import org.eclipse.jface.util.IPropertyChangeListener;
import org.eclipse.jface.util.PropertyChangeEvent;
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
		DirectoryFieldEditor pathEditor = getDirectoryEditor(PreferenceConstants.UFORADIANT_EXEC_PATH, "UFORadiant path");
		final FileFieldEditor binaryEditor = getFileEditor(PreferenceConstants.UFORADIANT_BINARY, "UFORadiant binary");
		addField(pathEditor);
		addField(binaryEditor);

		pathEditor.setPropertyChangeListener(new IPropertyChangeListener() {
			@Override
			public void propertyChange(PropertyChangeEvent event) {
				binaryEditor.setFilterPath(new File(event.getNewValue().toString()));
			}
		});
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
