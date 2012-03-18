package net.sourceforge.ufoai.md2viewer.opengl;

import org.lwjgl.opengl.GL11;

public class GridRenderer implements IRenderable {
    private static final int SIZE = 32;
    private static final int ROWS = 10 * SIZE;
    private static final int COLUMNS = 10 * SIZE;

    @Override
    public void render(IRenderParam param) {
	GL11.glDisable(GL11.GL_TEXTURE_2D);
	GL11.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	GL11.glBegin(GL11.GL_LINES);

	/* horizontal */
	for (int i = -ROWS; i <= ROWS; i += SIZE) {
	    GL11.glVertex3f(-ROWS, i, 0.0f);
	    GL11.glVertex3f(COLUMNS, i, 0.0f);
	}

	/* vertical */
	for (int i = -COLUMNS; i <= COLUMNS; i += SIZE) {
	    GL11.glVertex3f(i, -COLUMNS, 0.0f);
	    GL11.glVertex3f(i, ROWS, 0.0f);
	}

	GL11.glEnd();
	GL11.glEnable(GL11.GL_TEXTURE_2D);
    }
}
