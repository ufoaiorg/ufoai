package net.sourceforge.ufoai.md2viewer.opengl;

import javax.media.opengl.GL;

public interface IRenderable {
	/**
	 * Will render the object
	 */
	void render(GL gl, IRenderParam param);
}
