package net.sourceforge.ufoai.md2viewer.models.md2;

import java.io.IOException;

import net.sourceforge.ufoai.md2viewer.util.LittleEndianInputStream;
import net.sourceforge.ufoai.md2viewer.util.Vector3;

class MD2Vertex {
	private final Vector3 vertex;
	private final int normal;

	public MD2Vertex(final LittleEndianInputStream in, final Vector3 scale)
			throws IOException {
		final int xByte = in.read();
		final int yByte = in.read();
		final int zByte = in.read();
		final float x = xByte * scale.getX();
		final float y = yByte * scale.getY();
		final float z = zByte * scale.getZ();
		normal = in.getByte();
		vertex = new Vector3(x, y, z);
	}

	public Vector3 getVertex() {
		return vertex;
	}

	public int getNormal() {
		return normal;
	}

	@Override
	public String toString() {
		return "MD2Vertex [normal=" + normal + ", vertex=" + vertex + "]";
	}
}
