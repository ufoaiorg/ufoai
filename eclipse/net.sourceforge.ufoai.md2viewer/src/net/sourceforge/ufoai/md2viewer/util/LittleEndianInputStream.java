package net.sourceforge.ufoai.md2viewer.util;

import java.io.BufferedInputStream;
import java.io.EOFException;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;

public class LittleEndianInputStream extends FilterInputStream {
	private final static int size = 128;
	private long pos;

	public LittleEndianInputStream(final InputStream in) {
		super(getBufferedInputStream(in));
	}

	private static BufferedInputStream getBufferedInputStream(
			final InputStream in) {
		BufferedInputStream bufferedInputStream = new BufferedInputStream(in);
		bufferedInputStream.mark(size);
		return bufferedInputStream;
	}

	public byte getByte() throws IOException {
		final int temp = read();
		pos++;
		if (temp == -1)
			throw new EOFException();
		return (byte) temp;
	}

	public short getShort() throws IOException {
		final int byte1 = read();
		final int byte2 = read();
		pos += 2;
		if (byte2 == -1)
			throw new EOFException();
		return (short) (((byte2 << 24) >>> 16) + (byte1 << 24) >>> 24);
	}

	public int getInt() throws IOException {
		final int byte1 = read();
		final int byte2 = read();
		final int byte3 = read();
		final int byte4 = read();
		pos += 4;
		if (byte4 == -1) {
			throw new EOFException();
		}
		return (byte4 << 24) + ((byte3 << 24) >>> 8) + ((byte2 << 24) >>> 16)
				+ ((byte1 << 24) >>> 24);
	}

	public final float getFloat() throws IOException {
		return Float.intBitsToFloat(this.getInt());
	}

	public Vector3 getVector3() throws IOException {
		float x = this.getFloat();
		float y = this.getFloat();
		float z = this.getFloat();
		return new Vector3(x, y, z);
	}

	public String getString(int length) throws IOException {
		byte buf[] = new byte[length];
		for (int i = 0; i < length; i++) {
			buf[i] = getByte();
		}
		return new String(buf);
	}

	public void setPosition(long position) throws IOException {
		reset();
		pos = 0;
		for (long remaining = position; remaining > 0;) {
			long skip = skip(remaining);
			remaining -= skip;
		}

		pos = position;
	}

	public long getPosition() {
		return pos;
	}
}
