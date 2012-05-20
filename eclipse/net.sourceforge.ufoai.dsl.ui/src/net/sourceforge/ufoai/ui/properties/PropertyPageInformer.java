package net.sourceforge.ufoai.ui.properties;

import org.eclipse.emf.ecore.EObject;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.ui.IPageLayout;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.views.properties.PropertySheet;
import org.eclipse.xtext.ui.editor.XtextEditor;

public class PropertyPageInformer {

	public static void informPropertyView(XtextEditor editor, EObject object) {
		PropertySheet propertyView = null;

		// fetch the properties view
		try {
			propertyView = (PropertySheet) PlatformUI.getWorkbench().getActiveWorkbenchWindow().getActivePage()
					.findView(IPageLayout.ID_PROP_SHEET);
		} catch (Exception e) {
		}

		if (propertyView != null) {
			// make sure our editor is marked as active part
			// otherwise the selectionChanged-call will be ignored
			propertyView.partActivated(editor);
			// feed the view with our custom selection
			propertyView.selectionChanged(editor, constructSelection(object));
		}
	}

	private static ISelection constructSelection(final EObject object) {
		return new StructuredSelection() {
			@Override
			public Object[] toArray() {
				return new Object[] { new UFOAIPropertyInfo(object) };
			}
		};
	}
}
