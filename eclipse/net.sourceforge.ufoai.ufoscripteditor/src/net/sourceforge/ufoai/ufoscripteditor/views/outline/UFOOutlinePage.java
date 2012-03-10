package net.sourceforge.ufoai.ufoscripteditor.views.outline;

import net.sourceforge.ufoai.ufoscripteditor.editors.UFOScriptEditor;
import net.sourceforge.ufoai.ufoscripteditor.parser.UFOScriptBlock;

import org.eclipse.jface.text.BadLocationException;
import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.IRegion;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.TreeSelection;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.texteditor.IDocumentProvider;
import org.eclipse.ui.views.contentoutline.ContentOutlinePage;

public class UFOOutlinePage extends ContentOutlinePage {
	private IEditorInput editorInput;
	private final IDocumentProvider documentProvider;
	private final UFOScriptEditor ufoScriptEditor;

	public UFOOutlinePage(IDocumentProvider documentProvider,
			UFOScriptEditor ufoScriptEditor) {
		this.documentProvider = documentProvider;
		this.ufoScriptEditor = ufoScriptEditor;
	}

	@Override
	public void createControl(Composite parent) {
		super.createControl(parent);
		TreeViewer viewer = getTreeViewer();
		viewer.getTree().setLinesVisible(true);
		viewer.setContentProvider(new UFOContentProvider(documentProvider));
		viewer.setLabelProvider(new UFOLabelProvider());
		viewer.addSelectionChangedListener(this);
		viewer.setAutoExpandLevel(1);
		if (editorInput != null)
			viewer.setInput(editorInput);
	}

	@Override
	public void selectionChanged(final SelectionChangedEvent event) {
		super.selectionChanged(event);
		final TreeSelection selection = (TreeSelection) event.getSelection();
		if (selection.isEmpty())
			ufoScriptEditor.resetHighlightRange();
		else {
			try {
				UFOScriptBlock b = (UFOScriptBlock) selection.getFirstElement();
				IDocument doc = documentProvider.getDocument(editorInput);
				IRegion start = doc.getLineInformation(b.getStartLine());
				IRegion end = doc.getLineInformation(b.getStartLine()
						+ b.getLines());
				ufoScriptEditor.setHighlightRange(start.getOffset(), end
						.getOffset()
						- start.getOffset(), true);
			} catch (final IllegalArgumentException e) {
				ufoScriptEditor.resetHighlightRange();
			} catch (BadLocationException e) {
				ufoScriptEditor.resetHighlightRange();
			}
		}
	}

	public void setInput(IEditorInput editorInput) {
		this.editorInput = editorInput;
		update();
	}

	public void update() {
		final TreeViewer viewer = getTreeViewer();
		if (viewer != null) {
			final Control control = viewer.getControl();
			if (control != null && !control.isDisposed()) {
				control.setRedraw(false);
				viewer.setInput(editorInput);
				viewer.expandAll();
				control.setRedraw(true);
			}
		}
	}

}
