package net.sourceforge.ufoai.md2viewer.opengl;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;

public class GridRenderer implements IRenderable {
	private static final int SIZE = 32;
	private static final int ROWS = 10 * SIZE;
	private static final int COLUMNS = 10 * SIZE;

	@Override
	public void render(GL gl, IRenderParam param) {
		GL2 gl2 = gl.getGL2();
		gl2.glDisable(GL2.GL_TEXTURE_2D);
		gl2.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		gl2.glBegin(GL2.GL_LINES);

		/* horizontal */
		for (int i = -ROWS; i <= ROWS; i += SIZE) {
			gl2.glVertex3f(-ROWS, i, 0.0f);
			gl2.glVertex3f(COLUMNS, i, 0.0f);
		}

		/* vertical */
		for (int i = -COLUMNS; i <= COLUMNS; i += SIZE) {
			gl2.glVertex3f(i, -COLUMNS, 0.0f);
			gl2.glVertex3f(i, ROWS, 0.0f);
		}

		gl2.glEnd();
		gl2.glEnable(GL2.GL_TEXTURE_2D);
	}
}
