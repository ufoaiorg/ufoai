package net.sourceforge.ufoai.md2viewer.opengl;

import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.List;

import net.sourceforge.ufoai.md2viewer.util.Vector3;

import org.eclipse.swt.SWT;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.MouseWheelListener;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.opengl.GLCanvas;
import org.eclipse.swt.opengl.GLData;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.lwjgl.BufferUtils;
import org.lwjgl.LWJGLException;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GLContext;
import org.lwjgl.util.glu.GLU;

/**
 * Class that encapsulates the OpenGL rendering. Make sure to add the objects
 * you would like to render to the queue. Use {@link #addRenderable} for this.
 *
 * @see {@link IRenderable}
 * @see {@link IRenderParam}
 *
 * @author mattn
 */
public class OpenGLCanvas {
    private final GLCanvas canvas;
    private float camDist = -200;
    private Vector3 axisRot;
    private boolean drag = false;
    private final FloatBuffer rotation = BufferUtils.createFloatBuffer(16);
    private final List<IRenderable> renderables = new ArrayList<IRenderable>();
    private final List<IRenderParam> renderParams = new ArrayList<IRenderParam>();

    /**
     * @param parent
     *            {@link Composite}
     */
    public OpenGLCanvas(final Composite comp) {
	TextureLoader.init(comp.getDisplay());

	// this allows us to set particular properties for the GLCanvas
	GLData data = new GLData();
	data.doubleBuffer = true;

	Composite parent = new Composite(comp, SWT.NONE);
	parent.setLayout(new FillLayout());

	// instantiate the canvas
	this.canvas = new GLCanvas(parent, SWT.NONE, data);
	this.canvas.setCurrent();

	try {
	    GLContext.useContext(canvas);
	} catch (LWJGLException e) {
	    e.printStackTrace();
	}

	// we need the listener so we get the GL events
	this.canvas.addMouseWheelListener(new MouseWheelListener() {
	    @Override
	    public void mouseScrolled(MouseEvent e) {
		camDist -= e.count * 10;
		if (camDist > -16)
		    camDist = -16;
	    }
	});

	this.canvas.addMouseListener(new MouseListener() {
	    @Override
	    public void mouseUp(MouseEvent e) {
		drag = false;
	    }

	    @Override
	    public void mouseDown(MouseEvent e) {
		drag = true;
	    }

	    @Override
	    public void mouseDoubleClick(MouseEvent e) {
	    }
	});

	this.canvas.addMouseMoveListener(new MouseMoveListener() {
	    private float lastX = 0;
	    private float lastY = 0;

	    @Override
	    public void mouseMove(MouseEvent e) {
		if (!drag)
		    return;
		// Calculate the mouse delta as a vector in the XY plane, and
		// store the
		// current position for the next event.
		final Vector3 deltaPos = new Vector3(e.x - lastX, lastY - e.y,
			0.0f);
		lastX = e.x;
		lastY = e.y;

		// Calculate the axis of rotation. This is the mouse vector
		// crossed with the Z axis,
		// to give a rotation axis in the XY plane at right-angles to
		// the mouse delta.
		final Vector3 zAxis = new Vector3(0, 0, 1);
		axisRot = deltaPos.crossProduct(zAxis);
	    }
	});

	final Rectangle clientArea = parent.getClientArea();
	this.canvas.setSize(clientArea.width, clientArea.height);

	// fix the viewport when the user resizes the window
	this.canvas.addListener(SWT.Resize, new Listener() {
	    public void handleEvent(Event event) {
		Rectangle rectangle = canvas.getClientArea();
		reshape(rectangle.x, rectangle.y, rectangle.width,
			rectangle.height);
	    }
	});

	this.canvas.addPaintListener(new PaintListener() {
	    public void paintControl(PaintEvent paintevent) {
		display();
	    }
	});

	parent.addDisposeListener(new DisposeListener() {
	    @Override
	    public void widgetDisposed(DisposeEvent e) {
		dispose();
	    }
	});

	parent.getDisplay().asyncExec(new Runnable() {
	    public void run() {
		if (!canvas.isDisposed()) {
		    display();
		    canvas.getDisplay().asyncExec(this);
		}
	    }
	});
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

    public void display() {
	init();
	GL11.glClearColor(0.3f, 0.3f, 0.3f, 0.0f);
	GL11.glClear(GL11.GL_COLOR_BUFFER_BIT | GL11.GL_DEPTH_BUFFER_BIT);

	if (axisRot != null) {
	    GL11.glLoadIdentity();
	    GL11.glMultMatrix(rotation);
	    GL11.glRotatef(-2.0f, axisRot.getX(), axisRot.getY(),
		    axisRot.getZ());
	    GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX, rotation);

	    axisRot = null;
	}

	setCamera();

	final int length = renderables.size();
	for (int i = 0; i < length; i++) {
	    final IRenderable iRenderable = renderables.get(i);
	    final IRenderParam param = renderParams.get(i);
	    iRenderable.render(param);
	}

	canvas.swapBuffers();
	GL11.glFinish();

	final int glGetError = GL11.glGetError();
	if (glGetError != 0)
	    System.out.println("Error: " + glGetError);
    }

    public void dispose() {
	// TODO that is not the correct location to shut down the static texture
	// cache for all opened editors
	TextureLoader.get().shutdown();
    }

    public void init() {
	GL11.glClearColor(0.3f, 0.3f, 0.3f, 0.0f);
	GL11.glEnable(GL11.GL_DEPTH_TEST);
	GL11.glDepthFunc(GL11.GL_LEQUAL);

	GL11.glBlendFunc(GL11.GL_SRC_ALPHA, GL11.GL_ONE_MINUS_SRC_ALPHA);
	GL11.glAlphaFunc(GL11.GL_GREATER, 0.01f);
	GL11.glHint(GL11.GL_PERSPECTIVE_CORRECTION_HINT, GL11.GL_NICEST);
	GL11.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	GL11.glEnable(GL11.GL_ALPHA_TEST);
	GL11.glEnable(GL11.GL_BLEND);
	GL11.glMatrixMode(GL11.GL_MODELVIEW);
	GL11.glLoadIdentity();
	GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX, rotation);
    }

    public void reshape(final int x, final int y, final int width,
	    final int height) {
	canvas.setCurrent();
	try {
	    GLContext.useContext(canvas);
	} catch (LWJGLException e) {
	    e.printStackTrace();
	}
	GL11.glViewport(0, 0, width, height);
	setCamera();
    }

    /**
     * Set up the camera
     */
    private void setCamera() {
	final float previewFOV = 60.0f;
	GL11.glMatrixMode(GL11.GL_PROJECTION);
	GL11.glLoadIdentity();
	GLU.gluPerspective(previewFOV, 1.0f, 0.1f, 10000.0f);

	GL11.glMatrixMode(GL11.GL_MODELVIEW);
	GL11.glLoadIdentity();

	GL11.glTranslatef(0.0f, 0.0f, camDist);
	GL11.glMultMatrix(rotation);
    }

    public void redraw() {
	display();
    }
}
