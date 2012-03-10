package net.sourceforge.ufoai.md2viewer.models.md2;

import java.io.IOException;
import java.util.List;

import javax.media.opengl.GL;

import net.sourceforge.ufoai.md2viewer.models.IModel;
import net.sourceforge.ufoai.md2viewer.models.IModelFactory;
import net.sourceforge.ufoai.md2viewer.models.IModelSkin;
import net.sourceforge.ufoai.md2viewer.opengl.IRenderParam;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IProgressMonitor;

public class MD2Tag implements IModel {
	public static final IModelFactory FACTORY = new IModelFactory() {
		@Override
		public IModel create() {
			return new MD2Tag();
		}

		public String getExtension() {
			return "tag";
		}
	};

	private MD2Tag() {
	}

	@Override
	public void load(IFile file, IProgressMonitor pm) throws IOException {
	}

	@Override
	public void render(GL gl, IRenderParam param) {
	}

	@Override
	public List<IModelSkin> getSkins() {
		return null;
	}
}
