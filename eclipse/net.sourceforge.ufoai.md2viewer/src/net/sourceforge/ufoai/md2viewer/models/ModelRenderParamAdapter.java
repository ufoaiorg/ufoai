package net.sourceforge.ufoai.md2viewer.models;

import net.sourceforge.ufoai.md2viewer.opengl.RenderParamAdapter;

public class ModelRenderParamAdapter extends RenderParamAdapter implements
		IModelRenderParam {
	private int skin;
	private int frame;

	@Override
	public int getFrame() {
		return frame;
	}

	public void setFrame(int frame) {
		this.frame = frame;
	}

	@Override
	public int getSkin() {
		return skin;
	}

	public void setSkin(int skin) {
		this.skin = skin;
	}
}
