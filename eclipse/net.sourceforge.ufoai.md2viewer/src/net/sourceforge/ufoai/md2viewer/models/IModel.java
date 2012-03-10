package net.sourceforge.ufoai.md2viewer.models;

import java.io.IOException;
import java.io.InputStream;
import java.util.List;

import net.sourceforge.ufoai.md2viewer.opengl.IRenderable;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IProgressMonitor;

public interface IModel extends IRenderable {
	/**
	 * Loads a model from a given {@link InputStream}
	 *
	 * @throws IOException
	 *             If loading the model failed
	 */
	void load(IFile file, IProgressMonitor pm) throws IOException;

	/**
	 * @return Returns a list with the skins - might also return
	 *         <code>null</code>
	 */
	List<IModelSkin> getSkins();
}
