package net.sourceforge.ufoai.md2viewer.models.md2;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import net.sourceforge.ufoai.md2viewer.util.LittleEndianInputStream;
import net.sourceforge.ufoai.md2viewer.util.Vector3;

public class MD2Frame {
	private final List<MD2Vertex> vertexList = new ArrayList<MD2Vertex>();
	private final String name;
	private final Vector3 scale;
	private final Vector3 translate;

	public MD2Frame(final LittleEndianInputStream e, final MD2Header header)
			throws IOException {
		scale = e.getVector3();
		translate = e.getVector3();
		name = e.getString(16);
		final int numVerts = header.getNumVerts();
		for (int j = 0; j < numVerts; j++) {
			final MD2Vertex v = new MD2Vertex(e, scale);
			vertexList.add(v);
		}
	}

	public String getName() {
		return name;
	}

	public Vector3 getScale() {
		return scale;
	}

	public Vector3 getTranslate() {
		return translate;
	}

	public Vector3 getVertex(final int index) {
		return vertexList.get(index).getVertex();
	}

	@Override
	public String toString() {
		return "MD2Frame [name=" + name + ", scale=" + scale + ", translate="
				+ translate + ", vertexList=" + vertexList + "]";
	}
}
