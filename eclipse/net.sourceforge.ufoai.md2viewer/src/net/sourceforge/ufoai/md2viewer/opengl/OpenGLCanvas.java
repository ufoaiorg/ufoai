package net.sourceforge.ufoai.md2viewer.opengl;

import java.awt.event.MouseEvent;
import java.awt.event.MouseMotionListener;
import java.awt.event.MouseWheelEvent;
import java.awt.event.MouseWheelListener;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.media.opengl.GLAutoDrawable;
import javax.media.opengl.GLCapabilities;
import javax.media.opengl.GLEventListener;
import javax.media.opengl.GLProfile;
import javax.media.opengl.awt.GLCanvas;
import javax.media.opengl.glu.GLU;

import net.sourceforge.ufoai.md2viewer.util.Vector3;

import org.eclipse.swt.SWT;
import org.eclipse.swt.awt.SWT_AWT;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Composite;

import com.jogamp.opengl.util.GLBuffers;

/**
 * Class that encapsulates the OpenGL rendering. Make sure to add the objects
 * you would like to render to the queue. Use {@link #addRenderable} for this.
 *
 * @see {@link IRenderable}
 * @see {@link IRenderParam}
 *
 * @author mattn
 */
public class OpenGLCanvas implements GLEventListener {
	private final GLCapabilities glCapabilities;
	private final GLCanvas canvas;
	private float camDist = -18;
	private Vector3 axisRot;
	private final FloatBuffer rotation = FloatBuffer.allocate(16);
	private final List<IRenderable> renderables = new ArrayList<IRenderable>();
	private final List<IRenderParam> renderParams = new ArrayList<IRenderParam>();

	/**
	 * @param parent
	 *            {@link Composite}
	 */
	public OpenGLCanvas(final Composite parent) {
		TextureLoader.init(parent.getDisplay());

		// this allows us to set particular properties for the GLCanvas
		glCapabilities = new GLCapabilities(GLProfile.getDefault());

		glCapabilities.setDoubleBuffered(true);
		glCapabilities.setHardwareAccelerated(true);

		// instantiate the canvas
		this.canvas = new GLCanvas(glCapabilities);

		// we can't use the default Composite because using the AWT bridge
		// requires that it have the property of SWT.EMBEDDED
		final Composite composite = new Composite(parent, SWT.EMBEDDED);

		// set the layout so our canvas fills the whole control
		composite.setLayout(new FillLayout());

		// create the special frame bridge to AWT
		final java.awt.Frame glFrame = SWT_AWT.new_Frame(composite);

		// we need the listener so we get the GL events
		this.canvas.addGLEventListener(this);
		this.canvas.addMouseWheelListener(new MouseWheelListener() {
			@Override
			public void mouseWheelMoved(final MouseWheelEvent e) {
				camDist -= e.getWheelRotation() * 10;
				canvas.repaint();
			}
		});

		this.canvas.addMouseMotionListener(new MouseMotionListener() {
			private float lastX = 0;
			private float lastY = 0;

			@Override
			public void mouseMoved(final MouseEvent e) {
			}

			@Override
			public void mouseDragged(final MouseEvent e) {
				// Calculate the mouse delta as a vector in the XY plane, and
				// store the
				// current position for the next event.
				final Vector3 deltaPos = new Vector3(e.getX() - lastX, lastY
						- e.getY(), 0.0f);
				lastX = e.getX();
				lastY = e.getY();

				// Calculate the axis of rotation. This is the mouse vector
				// crossed with the Z axis,
				// to give a rotation axis in the XY plane at right-angles to
				// the mouse delta.
				final Vector3 zAxis = new Vector3(0, 0, 1);
				axisRot = deltaPos.crossProduct(zAxis);
				canvas.repaint();
			}
		});

		// finally, add our canvas as a child of the frame
		glFrame.add(this.canvas);

		final Rectangle clientArea = parent.getClientArea();
		this.canvas.setSize(clientArea.width, clientArea.height);
	}

	/**
	 * Add {@link IRenderable} and it's related {@link IRenderParam} to the
	 * render queue
	 *
	 * @param renderable
	 *            {@link IRenderable}. The renderable.
	 * @param renderParam
	 *            {@link IRenderParam}. This render param is forwarded to your
	 *            {@link IRenderable}. Can be <code>null</code> if your
	 *            renderable does not need this.
	 */
	public void addRenderable(final IRenderable renderable,
			final IRenderParam renderParam) {
		renderables.add(renderable);
		renderParams.add(renderParam);
	}

	@Override
	public void display(final GLAutoDrawable drawable) {
		final GL gl = drawable.getGL();
		if (!gl.getContext().isCurrent())
			throw new RuntimeException("no current");

		final GL2 gl2 = gl.getGL2();
		gl2.glLoadIdentity();
		gl.glClearColor(0.3f, 0.3f, 0.3f, 0.0f);
		gl.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT);

		if (axisRot != null) {
			gl2.glMultMatrixf(rotation);
			gl2
					.glRotatef(-2.0f, axisRot.getX(), axisRot.getY(), axisRot
							.getZ());
			gl2.glGetFloatv(GL2.GL_MODELVIEW_MATRIX, rotation);

			axisRot = null;
		}

		setCamera(gl);

		render(gl);

		drawable.swapBuffers();
		gl.glFinish();

		final int glGetError = gl.glGetError();
		if (glGetError != 0)
			System.out.println("Error: " + glGetError);
	}

	private void render(final GL gl) {
		final int length = renderables.size();
		for (int i = 0; i < length; i++) {
			final IRenderable iRenderable = renderables.get(i);
			final IRenderParam param = renderParams.get(i);
			iRenderable.render(gl, param);
		}
	}

	@Override
	public void dispose(final GLAutoDrawable drawable) {
		final GL gl = drawable.getGL();
		// TODO that is not the correct location to shut down the static texture
		// cache for all opened editors
		TextureLoader.get().shutdown(gl);
	}

	@Override
	public void init(final GLAutoDrawable drawable) {
		final boolean enableLight = false; // TODO
		final GL gl = drawable.getGL();
		gl.glClearColor(0.3f, 0.3f, 0.3f, 0.0f);
		gl.glEnable(GL.GL_DEPTH_TEST);
		gl.glDepthFunc(GL.GL_LEQUAL);

		final GL2 gl2 = gl.getGL2();

		if (enableLight) {
			// Set up the lights
			gl2.glEnable(GL2.GL_LIGHTING);

			gl2.glEnable(GL2.GL_LIGHT0);
			final float l0Amb[] = { 0.3f, 0.3f, 0.3f, 1.0f };
			final float l0Dif[] = { 1.0f, 1.0f, 1.0f, 1.0f };
			final float l0Pos[] = { 1.0f, 1.0f, 1.0f, 0.0f };
			gl2.glLightfv(GL2.GL_LIGHT0, GL2.GL_AMBIENT, GLBuffers
					.newDirectFloatBuffer(l0Amb));
			gl2.glLightfv(GL2.GL_LIGHT0, GL2.GL_DIFFUSE, GLBuffers
					.newDirectFloatBuffer(l0Dif));
			gl2.glLightfv(GL2.GL_LIGHT0, GL2.GL_POSITION, GLBuffers
					.newDirectFloatBuffer(l0Pos));

			gl2.glEnable(GL2.GL_LIGHT1);
			final float l1Dif[] = { 1.0f, 1.0f, 1.0f, 1.0f };
			final float l1Pos[] = { 0.0f, 0.0f, 1.0f, 0.0f };
			gl2.glLightfv(GL2.GL_LIGHT1, GL2.GL_DIFFUSE, GLBuffers
					.newDirectFloatBuffer(l1Dif));
			gl2.glLightfv(GL2.GL_LIGHT1, GL2.GL_POSITION, GLBuffers
					.newDirectFloatBuffer(l1Pos));
		}

		gl2.glBlendFunc(GL2.GL_SRC_ALPHA, GL2.GL_ONE_MINUS_SRC_ALPHA);
		gl2.glAlphaFunc(GL2.GL_GREATER, 0.01f);
		gl2.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		gl2.glEnable(GL2.GL_ALPHA_TEST);
		gl2.glEnable(GL2.GL_BLEND);
		gl2.glMatrixMode(GL2.GL_MODELVIEW);
		gl2.glLoadIdentity();
		gl2.glGetFloatv(GL2.GL_MODELVIEW_MATRIX, rotation);
	}

	@Override
	public void reshape(final GLAutoDrawable drawable, final int x,
			final int y, final int width, final int height) {
		final GL gl = drawable.getGL();
		gl.glViewport(0, 0, width, height);
		setCamera(gl);
	}

	/**
	 * Set up the camera
	 */
	private void setCamera(final GL gl) {
		final float previewFOV = 60.0f;
		final GL2 gl2 = gl.getGL2();
		gl2.glMatrixMode(GL2.GL_PROJECTION);
		gl2.glLoadIdentity();
		GLU.createGLU(gl).gluPerspective(previewFOV, 1, 0.1, 10000);

		gl2.glMatrixMode(GL2.GL_MODELVIEW);
		gl2.glLoadIdentity();

		gl2.glTranslatef(0.0f, 0.0f, camDist);
		gl2.glMultMatrixf(rotation);
	}

	/**
	 * Can be used to update the OpenGL canvas.
	 */
	public void repaint() {
		canvas.repaint();
	}
}
