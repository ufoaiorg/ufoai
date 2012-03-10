package net.sourceforge.ufoai.md2viewer.models;

import net.sourceforge.ufoai.md2viewer.opengl.IRenderParam;

public interface IModelRenderParam extends IRenderParam {
	int getSkin();

	int getFrame();
}
