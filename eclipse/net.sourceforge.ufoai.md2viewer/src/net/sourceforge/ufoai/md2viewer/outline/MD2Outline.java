package net.sourceforge.ufoai.md2viewer.outline;

import java.util.List;

import net.sourceforge.ufoai.md2viewer.models.IModel;
import net.sourceforge.ufoai.md2viewer.models.IModelSkin;

import org.eclipse.jface.viewers.IDoubleClickListener;
import org.eclipse.jface.viewers.ILabelProviderListener;
import org.eclipse.jface.viewers.ITableLabelProvider;
import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.views.contentoutline.ContentOutlinePage;

public class MD2Outline extends ContentOutlinePage {
	private final IModel model;
	private final IDoubleClickListener clickListener;

	public MD2Outline(IModel model, IDoubleClickListener clickListener) {
		this.model = model;
		this.clickListener = clickListener;
	}

	@Override
	public void createControl(Composite parent) {
		super.createControl(parent);
		TreeViewer viewer = getTreeViewer();
		viewer.setContentProvider(new MD2ContentProvider());
		viewer.setLabelProvider(new MD2LabelProvider());
		viewer.addSelectionChangedListener(this);
		viewer.setInput(model);
		viewer.addDoubleClickListener(clickListener);
	}

	private class MD2LabelProvider implements ITableLabelProvider {
		@Override
		public Image getColumnImage(Object arg0, int arg1) {
			return null;
		}

		@Override
		public String getColumnText(Object arg0, int arg1) {
			if (arg0 instanceof IModelSkin) {
				String path = ((IModelSkin) arg0).getPath();
				return path;
			}
			return null;
		}

		@Override
		public void addListener(ILabelProviderListener listener) {
		}

		@Override
		public void dispose() {
		}

		@Override
		public boolean isLabelProperty(Object element, String property) {
			return false;
		}

		@Override
		public void removeListener(ILabelProviderListener listener) {
		}
	}

	private class MD2ContentProvider implements ITreeContentProvider {
		@Override
		public void dispose() {
		}

		@Override
		public void inputChanged(Viewer viewer, Object oldInput, Object newInput) {
		}

		@Override
		public Object[] getChildren(Object arg0) {
			return null;
		}

		@Override
		public Object getParent(Object arg0) {
			return null;
		}

		@Override
		public boolean hasChildren(Object arg0) {
			return false;
		}

		@Override
		public Object[] getElements(Object arg0) {
			if (arg0 instanceof IModel) {
				List<IModelSkin> skins = ((IModel) arg0).getSkins();
				if (skins != null) {
					return skins.toArray();
				}
			}
			return new Object[] {};
		}
	}
}
