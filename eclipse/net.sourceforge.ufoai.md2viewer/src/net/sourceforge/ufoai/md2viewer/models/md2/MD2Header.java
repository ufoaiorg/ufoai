package net.sourceforge.ufoai.md2viewer.models.md2;

import java.io.IOException;

import net.sourceforge.ufoai.md2viewer.util.LittleEndianInputStream;

class MD2Header {
	private final int ident;
	private final int version;
	private final int skinWidth;
	private final int skinHeight;
	private final int frameSize;

	private final int numSkins;
	private final int numVerts;
	private final int numSt;
	private final int numTris;
	private final int numGLCmds;
	private final int numFrames;

	private final int ofsSkins;
	private final int ofsSt;
	private final int ofsTris;
	private final int ofsFrames;
	private final int ofsGLCmds;
	private final int ofsEnd;

	private static final int VERSION = 8;

	public MD2Header(LittleEndianInputStream e) throws IOException {
		this.ident = e.getInt();
		this.version = e.getInt();
		this.skinWidth = e.getInt();
		this.skinHeight = e.getInt();
		this.frameSize = e.getInt();

		this.numSkins = e.getInt();
		this.numVerts = e.getInt();
		this.numSt = e.getInt();
		this.numTris = e.getInt();
		this.numGLCmds = e.getInt();
		this.numFrames = e.getInt();

		this.ofsSkins = e.getInt();
		this.ofsSt = e.getInt();
		this.ofsTris = e.getInt();
		this.ofsFrames = e.getInt();
		this.ofsGLCmds = e.getInt();
		this.ofsEnd = e.getInt();

		e.reset();
		e.mark(this.ofsEnd);

		if (VERSION != version)
			throw new IOException("Invalid version found: " + version
					+ " (expected was " + VERSION + ")");
	}

	public int getIdent() {
		return ident;
	}

	public int getVersion() {
		return version;
	}

	public int getSkinWidth() {
		return skinWidth;
	}

	public int getSkinHeight() {
		return skinHeight;
	}

	public int getFrameSize() {
		return frameSize;
	}

	public int getNumSkins() {
		return numSkins;
	}

	public int getNumVerts() {
		return numVerts;
	}

	public int getNumSt() {
		return numSt;
	}

	public int getNumTris() {
		return numTris;
	}

	public int getNumGLCmds() {
		return numGLCmds;
	}

	public int getNumFrames() {
		return numFrames;
	}

	public int getOfsSkins() {
		return ofsSkins;
	}

	public int getOfsSt() {
		return ofsSt;
	}

	public int getOfsTris() {
		return ofsTris;
	}

	public int getOfsFrames() {
		return ofsFrames;
	}

	public int getOfsGLCmds() {
		return ofsGLCmds;
	}

	public int getOfsEnd() {
		return ofsEnd;
	}

	@Override
	public String toString() {
		return "MD2Header [frameSize=" + frameSize + ", ident=" + ident
				+ ", numFrames=" + numFrames + ", numGLCmds=" + numGLCmds
				+ ", numSkins=" + numSkins + ", numSt=" + numSt + ", numTris="
				+ numTris + ", numVerts=" + numVerts + ", ofsEnd=" + ofsEnd
				+ ", ofsFrames=" + ofsFrames + ", ofsGLCmds=" + ofsGLCmds
				+ ", ofsSkins=" + ofsSkins + ", ofsSt=" + ofsSt + ", ofsTris="
				+ ofsTris + ", skinHeight=" + skinHeight + ", skinWidth="
				+ skinWidth + ", version=" + version + "]";
	}
}
