package net.sourceforge.ufoai.md2viewer.opengl;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;

import org.eclipse.core.runtime.IPath;
import org.eclipse.swt.graphics.Device;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.Rectangle;

public class TextureLoader {

	private static Map<String, Integer> textures = new HashMap<String, Integer>();
	private static TextureLoader loader;
	private static Device dev;

	private TextureLoader() {
	}

	public static void init(final Device device) {
		dev = device;
	}

	public static synchronized TextureLoader get() {
		if (loader == null)
			loader = new TextureLoader();
		return loader;
	}

	public void shutdown(final GL gl) {
		final Collection<Integer> values = textures.values();
		final IntBuffer buf = IntBuffer.allocate(values.size());
		for (final Integer integer : values) {
			buf.put(integer);
		}
		textures.clear();
		gl.getGL2().glDeleteTextures(values.size(), buf);
	}

	public int load(final GL gl, final IPath skinPath) {
		final Integer integer = textures.get(skinPath.toString());
		if (integer != null)
			return integer;
		try {
			final IntBuffer buf = IntBuffer.allocate(1);
			final GL2 gl2 = gl.getGL2();
			gl2.glGenTextures(1, buf);
			final int texnum = buf.get(0);
			gl2.glBindTexture(GL2.GL_TEXTURE_2D, texnum);
			gl2.glTexParameteri(GL2.GL_TEXTURE_2D, GL2.GL_TEXTURE_WRAP_S,
					GL2.GL_REPEAT);
			gl2.glTexParameteri(GL2.GL_TEXTURE_2D, GL2.GL_TEXTURE_WRAP_T,
					GL2.GL_REPEAT);
			gl2.glTexParameteri(GL2.GL_TEXTURE_2D, GL2.GL_TEXTURE_MAG_FILTER,
					GL2.GL_NEAREST);
			gl2.glTexParameteri(GL2.GL_TEXTURE_2D, GL2.GL_TEXTURE_MIN_FILTER,
					GL2.GL_NEAREST);

			final Image texture = getTexture(skinPath);
			final byte[] rgba = getRGBA(texture);
			final ByteBuffer data = ByteBuffer.wrap(rgba);

			Rectangle bounds = texture.getBounds();
			gl2.glTexImage2D(GL2.GL_TEXTURE_2D, 0, GL2.GL_RGBA, bounds.width,
					bounds.height, 0, GL2.GL_RGBA, GL2.GL_UNSIGNED_BYTE, data);

			textures.put(skinPath.toString(), texnum);
			return texnum;
		} catch (final Exception e) {
			System.out.println(e);
			return -1;
		}
	}

	private byte[] getRGBA(Image texture) {
		ImageData imageData = texture.getImageData();
		if (imageData.depth == 32)
			return imageData.data;
		else if (imageData.depth == 24) {
			ByteArrayOutputStream s = new ByteArrayOutputStream(imageData.width
					* imageData.height * 4);
			byte[] data = imageData.data;
			for (int i = 0; i < data.length; i += 3) {
				s.write(data, i, 3);
				s.write(255);
			}
			return s.toByteArray();
		}
		throw new RuntimeException("Not yet implemented for depth "
				+ imageData.depth);
	}

	private Image getTexture(final IPath skinPath) {
		try {
			final Image texture = new Image(dev, skinPath.addFileExtension(
					"png").toString());
			return texture;
		} catch (final Exception e) {
			final Image texture = new Image(dev, skinPath.addFileExtension(
					"jpg").toString());
			return texture;
		}
	}
}
