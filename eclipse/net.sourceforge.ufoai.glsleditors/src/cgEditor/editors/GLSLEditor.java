package cgEditor.editors;

import org.eclipse.jface.util.IPropertyChangeListener;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.ui.editors.text.TextEditor;

import cgEditor.CgEditorPlugin;
import cgEditor.preferences.PreferenceConstants;

public class GLSLEditor extends TextEditor implements IPropertyChangeListener {
	public GLSLEditor() {
		super();
		setSourceViewerConfiguration(new GLSLConfiguration());

		CgEditorPlugin.getDefault().getPreferenceStore()
				.addPropertyChangeListener(this);
	}

	@Override
	public void dispose() {
		super.dispose();
	}

	@Override
	protected void handlePreferenceStoreChanged(PropertyChangeEvent event) {
		super.handlePreferenceStoreChanged(event);
	}

	@Override
	protected boolean affectsTextPresentation(PropertyChangeEvent event) {
		boolean affect = false;
		for (int i = 0; i < PreferenceConstants.PREFERENCES.length; i++) {
			affect = affect
					|| event.getProperty().equals(
							PreferenceConstants.PREFERENCES[i][0])
					|| event.getProperty().equals(
							PreferenceConstants.PREFERENCES[i][1])
					|| event.getProperty().equals(
							PreferenceConstants.PREFERENCES[i][2]);
		}
		return affect || super.affectsTextPresentation(event);
	}

	public void propertyChange(PropertyChangeEvent event) {
		handlePreferenceStoreChanged(event);
	}
}
