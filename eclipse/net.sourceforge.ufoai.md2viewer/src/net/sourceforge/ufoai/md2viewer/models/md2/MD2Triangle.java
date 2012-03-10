package net.sourceforge.ufoai.md2viewer.models.md2;

import java.io.IOException;
import java.util.Arrays;

import net.sourceforge.ufoai.md2viewer.util.LittleEndianInputStream;

class MD2Triangle {
	private final short indexVerts[] = new short[3];
	private final short indexST[] = new short[3];

	public MD2Triangle(LittleEndianInputStream e, MD2Header header)
			throws IOException {
		for (int i = 0; i < 3; i++)
			indexVerts[i] = e.getShort();
		for (int i = 0; i < 3; i++)
			indexST[i] = e.getShort();
	}

	public short getIndexVerts(int index) {
		return indexVerts[index];
	}

	public short getIndexST(int index) {
		return indexST[index];
	}

	@Override
	public String toString() {
		return "MD2Triangle [indexST=" + Arrays.toString(indexST)
				+ ", indexVerts=" + Arrays.toString(indexVerts) + "]";
	}
}
