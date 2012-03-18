package net.sourceforge.ufoai.md2viewer.models.md2;

import java.io.IOException;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.List;

import net.sourceforge.ufoai.md2viewer.models.IModel;
import net.sourceforge.ufoai.md2viewer.models.IModelFactory;
import net.sourceforge.ufoai.md2viewer.models.IModelRenderParam;
import net.sourceforge.ufoai.md2viewer.models.IModelSkin;
import net.sourceforge.ufoai.md2viewer.opengl.IRenderParam;
import net.sourceforge.ufoai.md2viewer.opengl.TextureLoader;
import net.sourceforge.ufoai.md2viewer.util.LittleEndianInputStream;
import net.sourceforge.ufoai.md2viewer.util.Vector3;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.GL11;

public class MD2Model implements IModel {
    private final List<MD2TexCoord> texCoordList = new ArrayList<MD2TexCoord>();
    private final List<MD2Frame> framesList = new ArrayList<MD2Frame>();
    private final List<IModelSkin> skinList = new ArrayList<IModelSkin>();
    private final List<MD2Triangle> trisList = new ArrayList<MD2Triangle>();
    private MD2Header header;
    private float[] texCoords;
    private float[] vertices;
    private IFile file;
    private int oldFrame;

    private MD2Model() {
    }

    public static final IModelFactory FACTORY = new IModelFactory() {
	@Override
	public IModel create() {
	    return new MD2Model();
	}

	public String getExtension() {
	    return "md2";
	}
    };
    private int outIndexs[];
    private int numVerts;

    @Override
    public void load(final IFile file, final IProgressMonitor pm)
	    throws IOException {
	this.file = file;
	if (file == null)
	    throw new IllegalArgumentException("file is null");

	try {
	    final LittleEndianInputStream e = new LittleEndianInputStream(
		    file.getContents());
	    header = new MD2Header(e);
	    loadSkins(e);
	    loadFrames(e);
	    loadTexcoords(e);
	    loadTris(e);

	    buildBuffers();
	} catch (final CoreException e) {
	    throw new IOException(e);
	}
    }

    private void buildBuffers() {
	final int numIndexes = header.getNumTris() * 3;
	final int tempIndex[] = new int[numIndexes];
	final int tempSTIndex[] = new int[numIndexes];
	for (int i = 0; i < header.getNumTris(); i++) {
	    final MD2Triangle md2Triangle = trisList.get(i);
	    for (int j = 0; j < 3; j++) {
		tempIndex[i * 3 + j] = md2Triangle.getIndexVerts(j);
		tempSTIndex[i * 3 + j] = md2Triangle.getIndexST(j);
	    }
	}

	numVerts = 0;
	outIndexs = new int[numIndexes];
	final int indRemap[] = new int[numIndexes];

	for (int i = 0; i < numIndexes; i++)
	    indRemap[i] = -1;

	for (int i = 0; i < numIndexes; i++) {
	    if (indRemap[i] != -1)
		continue;

	    /* remap duplicates */
	    for (int j = i + 1; j < numIndexes; j++) {
		if (tempIndex[j] != tempIndex[i])
		    continue;
		final MD2TexCoord texCoord = texCoordList.get(tempSTIndex[j]);
		final MD2TexCoord texCoord2 = texCoordList.get(tempSTIndex[i]);
		if (!texCoord.equals(texCoord2))
		    continue;

		indRemap[j] = i;
		outIndexs[j] = numVerts;
	    }

	    /* add unique vertex */
	    indRemap[i] = i;
	    outIndexs[i] = numVerts++;
	}

	for (int i = 0; i < numIndexes; i++) {
	    if (indRemap[i] == i)
		continue;

	    outIndexs[i] = outIndexs[indRemap[i]];
	}

	final float isw = 1.0f / header.getSkinWidth();
	final int numFrames = header.getNumFrames();

	texCoords = new float[numVerts * 2];
	vertices = new float[numVerts * 3 * numFrames];

	for (int j = 0; j < numIndexes; j++) {
	    final int index = tempSTIndex[indRemap[j]];
	    final MD2TexCoord md2TexCoord = texCoordList.get(index);
	    final int k = outIndexs[j] * 2;
	    texCoords[k + 0] = (md2TexCoord.getS() + 0.5f) * isw;
	    texCoords[k + 1] = (md2TexCoord.getT() + 0.5f) * isw;
	}

	for (int i = 0; i < numFrames; i++) {
	    final MD2Frame md2Frame = framesList.get(i);
	    final Vector3 translate = md2Frame.getTranslate();
	    for (int j = 0; j < numIndexes; j++) {
		final int index = tempIndex[indRemap[j]];
		final int k = outIndexs[j] * 3 + (i * numVerts);
		final Vector3 vertex = md2Frame.getVertex(index);
		final Vector3 floats = vertex.add(translate);
		vertices[k + 0] = floats.getX();
		vertices[k + 1] = floats.getY();
		vertices[k + 2] = floats.getZ();
	    }
	}
    }

    private void loadTris(final LittleEndianInputStream e) throws IOException {
	e.setPosition(header.getOfsTris());
	final int numTris = header.getNumTris();
	for (int i = 0; i < numTris; i++) {
	    trisList.add(new MD2Triangle(e, header));
	}
    }

    private void loadFrames(final LittleEndianInputStream e) throws IOException {
	final int numFrames = header.getNumFrames();
	for (int i = 0; i < numFrames; i++) {
	    e.setPosition(header.getOfsFrames() + i * header.getFrameSize());
	    framesList.add(new MD2Frame(e, header));
	}
    }

    private void loadTexcoords(final LittleEndianInputStream e)
	    throws IOException {
	e.setPosition(header.getOfsSt());
	for (int i = 0; i < header.getNumSt(); i++) {
	    texCoordList.add(new MD2TexCoord(e));
	}
    }

    private void loadSkins(final LittleEndianInputStream e) throws IOException {
	e.setPosition(header.getOfsSkins());
	final int numSkins = header.getNumSkins();
	for (int i = 0; i < numSkins; i++) {
	    final String skin = e.getString(64).trim();
	    skinList.add(new MD2ModelSkin(i, skin));
	}
    }

    @Override
    public void render(final IRenderParam param) {
	final IModelRenderParam modelParam = (IModelRenderParam) param;

	if (modelParam.isWireFrame()) {
	    GL11.glDisable(GL11.GL_TEXTURE_2D);
	    GL11.glPolygonMode(GL11.GL_FRONT, GL11.GL_LINE);
	    GL11.glPolygonMode(GL11.GL_BACK, GL11.GL_LINE);
	} else {
	    final int texnum = getSkin(modelParam.getSkin());
	    GL11.glEnable(GL11.GL_TEXTURE_2D);
	    GL11.glBindTexture(GL11.GL_TEXTURE_2D, texnum);
	}

	GL11.glEnableClientState(GL11.GL_VERTEX_ARRAY);
	GL11.glEnableClientState(GL11.GL_TEXTURE_COORD_ARRAY);

	if (framesList.size() == 1)
	    getVerticesForStaticMesh();
	else
	    getVerticesLerp(modelParam.getFrame());

	GL11.glDrawArrays(GL11.GL_TRIANGLES, 0, header.getNumTris() * 3);

	GL11.glDisableClientState(GL11.GL_VERTEX_ARRAY);
	GL11.glDisableClientState(GL11.GL_TEXTURE_COORD_ARRAY);

	if (modelParam.isWireFrame()) {
	    GL11.glEnable(GL11.GL_TEXTURE_2D);
	}
    }

    private void getVerticesForStaticMesh() {
	final int numTris = header.getNumTris();
	final int numVerts = numTris * 3;
	final FloatBuffer vertBuf = BufferUtils.createFloatBuffer(numVerts * 3);
	final FloatBuffer texBuf = BufferUtils.createFloatBuffer(numVerts * 2);
	for (int i = 0; i < numTris; i++) { /* draw the tris */
	    for (int j = 0; j < 3; j++) {
		final int arrayIndex = 3 * i + j;
		final int meshIndex = outIndexs[arrayIndex];
		texBuf.put(texCoords[meshIndex * 2 + 0]);
		texBuf.put(texCoords[meshIndex * 2 + 1]);
		vertBuf.put(vertices[meshIndex * 3 + 0]);
		vertBuf.put(vertices[meshIndex * 3 + 1]);
		vertBuf.put(vertices[meshIndex * 3 + 2]);
	    }
	}

	vertBuf.flip();
	texBuf.flip();

	GL11.glVertexPointer(3, 0, vertBuf);
	GL11.glTexCoordPointer(2, 0, texBuf);
    }

    private void getVerticesLerp(final int frame) {
	final float[] array = vertices;
	final MD2Frame old = framesList.get(oldFrame);
	final MD2Frame newF = framesList.get(frame);

	final float backlerp = getBackLerp(frame);
	final float frontlerp = 1.0f - backlerp;
	final Vector3 move = new Vector3(backlerp * old.getTranslate().getX()
		+ frontlerp * newF.getTranslate().getX(), backlerp
		* old.getTranslate().getY() + frontlerp
		* newF.getTranslate().getY(), backlerp
		* old.getTranslate().getZ() + frontlerp
		* newF.getTranslate().getZ());

	final List<Vector3> meshVerts = new ArrayList<Vector3>(numVerts);
	for (int i = 0; i < numVerts; i++) { /* lerp the verts */
	    final int ov = oldFrame * numVerts + i;
	    final int v = frame * numVerts + i;
	    final float x = move.getX() + array[ov + 0] * backlerp
		    + array[v + 0] * frontlerp;
	    final float y = move.getY() + array[ov + 1] * backlerp
		    + array[v + 1] * frontlerp;
	    final float z = move.getZ() + array[ov + 2] * backlerp
		    + array[v + 2] * frontlerp;
	    meshVerts.add(new Vector3(x, y, z));
	}

	final int numVerts = header.getNumTris() * 3;
	final FloatBuffer vertBuf = BufferUtils.createFloatBuffer(numVerts * 3);
	final FloatBuffer texBuf = BufferUtils.createFloatBuffer(numVerts * 2);
	for (int i = 0; i < header.getNumTris(); i++) { /* draw the tris */
	    for (int j = 0; j < 3; j++) {
		final int arrayIndex = 3 * i + j;
		final int meshIndex = outIndexs[arrayIndex];
		texBuf.put(texCoords[meshIndex + 0]);
		texBuf.put(texCoords[meshIndex + 1]);
		vertBuf.put(meshVerts.get(meshIndex).getFloats());
	    }
	}

	vertBuf.flip();
	texBuf.flip();

	GL11.glVertexPointer(3, 0, vertBuf);
	GL11.glTexCoordPointer(2, 0, texBuf);

	oldFrame = frame;
    }

    private float getBackLerp(final int frame) {
	// backlerp = 1.0 - as->dt / as->time;
	// TODO: implement the lerping here
	return 1.0f;
    }

    private int getSkin(final int skin) {
	final String image = skinList.get(skin).getPath();
	if (image.startsWith(".")) {
	    final IPath modelPath = file.getLocation();
	    final IPath modelDirectory = modelPath.removeLastSegments(1);
	    final IPath skinPath = modelDirectory.addTrailingSeparator()
		    .append(image.substring(1));
	    final int texnum = TextureLoader.get().load(skinPath);
	    return texnum;
	} else {
	    System.out
		    .println("Warning: model does not use the . notation for the skin path");
	    return TextureLoader.get().load(new Path(image));
	}
    }

    @Override
    public List<IModelSkin> getSkins() {
	return skinList;
    }
}
