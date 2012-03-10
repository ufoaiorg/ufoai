package net.sourceforge.ufoai.md2viewer.models.md2;

import java.io.IOException;

import net.sourceforge.ufoai.md2viewer.util.LittleEndianInputStream;

class MD2TexCoord {
	private final short s;
	private final short t;

	public MD2TexCoord(LittleEndianInputStream in) throws IOException {
		s = in.getShort();
		t = in.getShort();
	}

	public short getS() {
		return s;
	}

	public short getT() {
		return t;
	}

	@Override
	public int hashCode() {
		final int prime = 31;
		int result = 1;
		result = prime * result + s;
		result = prime * result + t;
		return result;
	}

	@Override
	public boolean equals(Object obj) {
		if (this == obj)
			return true;
		if (obj == null)
			return false;
		if (getClass() != obj.getClass())
			return false;
		MD2TexCoord other = (MD2TexCoord) obj;
		if (s != other.s)
			return false;
		if (t != other.t)
			return false;
		return true;
	}

	@Override
	public String toString() {
		return "MD2TexCoord [s=" + s + ", t=" + t + "]";
	}
}
