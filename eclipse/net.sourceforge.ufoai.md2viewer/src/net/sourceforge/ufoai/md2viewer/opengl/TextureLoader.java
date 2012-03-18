package net.sourceforge.ufoai.md2viewer.opengl;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.IPath;
import org.eclipse.swt.graphics.Device;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.Rectangle;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.GL11;

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

    public void shutdown() {
	final Collection<Integer> values = textures.values();
	for (final Integer integer : values) {
	    GL11.glDeleteTextures(integer);
	}
	textures.clear();
    }

    public int load(final IPath skinPath) {
	final Integer integer = textures.get(skinPath.toString());
	if (integer != null)
	    return integer;
	try {
	    final int texnum = GL11.glGenTextures();
	    GL11.glBindTexture(GL11.GL_TEXTURE_2D, texnum);
	    GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_WRAP_S,
		    GL11.GL_REPEAT);
	    GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_WRAP_T,
		    GL11.GL_REPEAT);
	    GL11.glTexParameteri(GL11.GL_TEXTURE_2D,
		    GL11.GL_TEXTURE_MAG_FILTER, GL11.GL_NEAREST);
	    GL11.glTexParameteri(GL11.GL_TEXTURE_2D,
		    GL11.GL_TEXTURE_MIN_FILTER, GL11.GL_NEAREST);

	    final Image texture = getTexture(skinPath);
	    final byte[] rgba = getRGBA(texture);
	    final ByteBuffer data = BufferUtils.createByteBuffer(rgba.length);
	    data.put(rgba);
	    data.flip();

	    Rectangle bounds = texture.getBounds();
	    GL11.glTexImage2D(GL11.GL_TEXTURE_2D, 0, GL11.GL_RGBA,
		    bounds.width, bounds.height, 0, GL11.GL_RGBA,
		    GL11.GL_UNSIGNED_BYTE, data);

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
