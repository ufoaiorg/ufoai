package net.sourceforge.ufoai.md2viewer.models.md2;

import net.sourceforge.ufoai.md2viewer.models.IModelSkin;

class MD2ModelSkin implements IModelSkin {
	private final int index;
	private final String path;

	public MD2ModelSkin(int index, String path) {
		this.index = index;
		this.path = path;
	}

	@Override
	public int getIndex() {
		return index;
	}

	@Override
	public String getPath() {
		return path;
	}

	@Override
	public String toString() {
		return path;
	}
}
