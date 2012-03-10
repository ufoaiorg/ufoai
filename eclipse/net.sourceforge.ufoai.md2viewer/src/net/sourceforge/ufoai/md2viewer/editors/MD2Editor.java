package net.sourceforge.ufoai.md2viewer.editors;

import net.sourceforge.ufoai.md2viewer.models.IModel;
import net.sourceforge.ufoai.md2viewer.models.IModelSkin;
import net.sourceforge.ufoai.md2viewer.models.ModelFactory;
import net.sourceforge.ufoai.md2viewer.models.ModelRenderParamAdapter;
import net.sourceforge.ufoai.md2viewer.opengl.GridRenderer;
import net.sourceforge.ufoai.md2viewer.opengl.OpenGLCanvas;
import net.sourceforge.ufoai.md2viewer.outline.MD2Outline;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.viewers.DoubleClickEvent;
import org.eclipse.jface.viewers.IDoubleClickListener;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.TreeSelection;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.part.EditorPart;
import org.eclipse.ui.views.contentoutline.IContentOutlinePage;

public class MD2Editor extends EditorPart {
	private final ModelRenderParamAdapter renderParam = new ModelRenderParamAdapter();
	private String name;
	private IModel model;
	private boolean dirty;
	private Object myOutlinePage;
	private OpenGLCanvas openGLCanvas;

	public MD2Editor() {
	}

	@Override
	public void doSave(final IProgressMonitor arg0) {
		setDirty(false);
	}

	@Override
	public void doSaveAs() {
	}

	@Override
	public void init(final IEditorSite site, final IEditorInput input)
			throws PartInitException {
		setSite(site);
		setInput(input);
		name = input.getName();
		if (!(input instanceof IFileEditorInput))
			throw new PartInitException("IFileEditorInput expected");

		final IFileEditorInput fileInput = (IFileEditorInput) input;
		final IFile file = fileInput.getFile();
		try {
			model = ModelFactory.get().get(file.getFileExtension());
			model.load(file, null);
		} catch (final Exception e) {
			throw new PartInitException(e.getMessage());
		}
	}

	@Override
	public boolean isDirty() {
		return dirty;
	}

	public void setDirty(final boolean dirty) {
		this.dirty = dirty;
		firePropertyChange(PROP_DIRTY);
	}

	@Override
	public boolean isSaveAsAllowed() {
		// TODO: saving should be possible (e.g. modifying the skins)
		return false;
	}

	@Override
	public void createPartControl(final Composite parent) {
		setPartName(name);
		openGLCanvas = new OpenGLCanvas(parent);
		openGLCanvas.addRenderable(new GridRenderer(), null);
		openGLCanvas.addRenderable(model, renderParam);
	}

	@Override
	public void setFocus() {
	}

	@Override
	public Object getAdapter(final Class required) {
		if (IContentOutlinePage.class.equals(required)) {
			if (myOutlinePage == null) {
				myOutlinePage = new MD2Outline(model, new SkinChanger());
			}
			return myOutlinePage;
		}
		return super.getAdapter(required);
	}

	private class SkinChanger implements IDoubleClickListener {
		public void doubleClick(final DoubleClickEvent event) {
			final ISelection selection = event.getSelection();
			if (selection instanceof TreeSelection) {
				final Object firstElement = ((TreeSelection) selection)
						.getFirstElement();
				if (firstElement instanceof IModelSkin) {
					final int index = ((IModelSkin) firstElement).getIndex();
					renderParam.setSkin(index);
					openGLCanvas.repaint();
				}
			}
		}
	}
}
