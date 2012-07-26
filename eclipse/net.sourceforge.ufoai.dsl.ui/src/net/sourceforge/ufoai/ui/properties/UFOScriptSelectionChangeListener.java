package net.sourceforge.ufoai.ui.properties;

import org.eclipse.emf.ecore.EObject;
import org.eclipse.jface.text.ITextSelection;
import org.eclipse.jface.viewers.IPostSelectionProvider;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.xtext.nodemodel.ICompositeNode;
import org.eclipse.xtext.nodemodel.ILeafNode;
import org.eclipse.xtext.nodemodel.util.NodeModelUtils;
import org.eclipse.xtext.parser.IParseResult;
import org.eclipse.xtext.resource.XtextResource;
import org.eclipse.xtext.ui.editor.XtextEditor;
import org.eclipse.xtext.util.concurrent.IUnitOfWork;

public class UFOScriptSelectionChangeListener implements ISelectionChangedListener {
	private final XtextEditor editor;

	public UFOScriptSelectionChangeListener(XtextEditor editor) {
		this.editor = editor;
	}

	public void selectionChanged(SelectionChangedEvent event) {
		try {
			ISelection selection = event.getSelection();
			if (!selection.isEmpty() && selection instanceof ITextSelection) {
				final ITextSelection textSel = (ITextSelection) selection;
				// determine the Model element at the offset and invoke the
				// invoke the information of the properties view

				editor.getDocument().readOnly(new IUnitOfWork.Void<XtextResource>() {
					@Override
					public void process(XtextResource resource) throws Exception {
						IParseResult parseResult = resource.getParseResult();
						if (parseResult == null)
							return;
						ICompositeNode rootNode = parseResult.getRootNode();
						int offset = textSel.getOffset();
						ILeafNode node = NodeModelUtils.findLeafNodeAtOffset(rootNode, offset);
						if (node == null)
							return;
						EObject object = node.getSemanticElement();

						PropertyPageInformer.informPropertyView(editor, object);
					}
				});

			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	/**
	 * install and uninstall is basically copied from
	 * AbstractSelectionChangedListener
	 */
	public void install(ISelectionProvider selectionProvider) {
		if (selectionProvider == null)
			return;

		if (selectionProvider instanceof IPostSelectionProvider) {
			IPostSelectionProvider provider = (IPostSelectionProvider) selectionProvider;
			provider.addPostSelectionChangedListener(this);
		} else {
			selectionProvider.addSelectionChangedListener(this);
		}
	}

	public void uninstall(ISelectionProvider selectionProvider) {
		if (selectionProvider == null)
			return;

		if (selectionProvider instanceof IPostSelectionProvider) {
			IPostSelectionProvider provider = (IPostSelectionProvider) selectionProvider;
			provider.removePostSelectionChangedListener(this);
		} else {
			selectionProvider.removeSelectionChangedListener(this);
		}
	}
}
