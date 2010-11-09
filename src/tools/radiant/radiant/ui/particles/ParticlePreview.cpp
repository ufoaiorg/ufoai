#include "ParticlePreview.h"

#include "gtkutil/GLWidgetSentry.h"
#include "gtkutil/image.h"
#include "os/path.h"
#include "../../referencecache/referencecache.h"
#include "math/aabb.h"
#include "../Icons.h"

#include <gtk/gtk.h>

namespace ui
{
	/* CONSTANTS */
	namespace
	{
		const GLfloat PREVIEW_FOV = 60;
	}

	// Construct the widgets
	ParticlePreview::ParticlePreview () :
		_widget(gtk_frame_new(NULL)), _glWidget(true)
	{
		// Main vbox - above is the GL widget, below is the toolbar
		GtkWidget* vbx = gtk_vbox_new(FALSE, 0);

		// Cast the GLWidget object to GtkWidget for further use
		GtkWidget* glWidget = _glWidget;
		gtk_box_pack_start(GTK_BOX(vbx), glWidget, TRUE, TRUE, 0);

		setSize(200);

		// Connect up the signals
		gtk_widget_set_events(glWidget, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
				| GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
		g_signal_connect(G_OBJECT(glWidget), "expose-event", G_CALLBACK(callbackGLDraw), this);
		g_signal_connect(G_OBJECT(glWidget), "motion-notify-event", G_CALLBACK(callbackGLMotion), this);
		g_signal_connect(G_OBJECT(glWidget), "scroll-event", G_CALLBACK(callbackGLScroll), this);

		// Pack into a frame and return
		gtk_container_add(GTK_CONTAINER(_widget), vbx);

		_particle = NULL;
	}

	// free the loaded model
	ParticlePreview::~ParticlePreview ()
	{
	}

	// Set the size request for the widget
	void ParticlePreview::setSize (int size)
	{
		GtkWidget* glWidget = _glWidget;
		gtk_widget_set_size_request(glWidget, size, size);
	}

	// Initialise the preview GL stuff
	void ParticlePreview::initialisePreview ()
	{
		GtkWidget* glWidget = _glWidget;
		// Grab the GL widget with sentry object
		gtkutil::GLWidgetSentry sentry(glWidget);

		// Clear the window
		glClearColor(0.0, 0.0, 0.0, 0);
		glClearDepth(100.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Set up the camera
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		GlobalOpenGL().perspective(PREVIEW_FOV, 1, 0.1, 10000);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		// Set up the lights
		glEnable(GL_LIGHTING);

		glEnable(GL_LIGHT0);
		GLfloat l0Amb[] = { 0.3, 0.3, 0.3, 1.0 };
		GLfloat l0Dif[] = { 1.0, 1.0, 1.0, 1.0 };
		GLfloat l0Pos[] = { 1.0, 1.0, 1.0, 0.0 };
		glLightfv(GL_LIGHT0, GL_AMBIENT, l0Amb);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, l0Dif);
		glLightfv(GL_LIGHT0, GL_POSITION, l0Pos);

		glEnable(GL_LIGHT1);
		GLfloat l1Dif[] = { 1.0, 1.0, 1.0, 1.0 };
		GLfloat l1Pos[] = { 0.0, 0.0, 1.0, 0.0 };
		glLightfv(GL_LIGHT1, GL_DIFFUSE, l1Dif);
		glLightfv(GL_LIGHT1, GL_POSITION, l1Pos);
		gtk_widget_queue_draw(glWidget);
	}

	// Set the particle, this also resets the camera

	void ParticlePreview::setParticle (IParticleDefinition* particle)
	{
		_particle = particle;
		// Redraw
		GtkWidget* glWidget = _glWidget;
		gtk_widget_queue_draw(glWidget);
	}

	IParticleDefinition* ParticlePreview::getParticle ()
	{
		return _particle;
	}

	/* GTK CALLBACKS */

	void ParticlePreview::callbackGLDraw (GtkWidget* widget, GdkEventExpose* ev, ParticlePreview* self)
	{
		GtkWidget* glWidget = self->_glWidget;
		// Create scoped sentry object to swap the GLWidget's buffers
		gtkutil::GLWidgetSentry sentry(glWidget);

		// Set up the render
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);

		// Premultiply with the translations
		glLoadIdentity();
		glTranslatef(0, 0, self->_camDist); // camera translation
		glMultMatrixf(self->_rotation); // post multiply with rotations

		// Render the actual model.
		glEnable(GL_LIGHTING);

		// TODO: mattn - activate the rendering again
		//self->_particle->render();
	}

	void ParticlePreview::callbackGLMotion (GtkWidget* widget, GdkEventMotion* ev, ParticlePreview* self)
	{
		if (ev->state & GDK_BUTTON1_MASK) { // dragging with mouse button
			static gdouble _lastX = ev->x;
			static gdouble _lastY = ev->y;

			// Calculate the mouse delta as a vector in the XY plane, and store the
			// current position for the next event.
			Vector3 deltaPos(ev->x - _lastX, _lastY - ev->y, 0);
			_lastX = ev->x;
			_lastY = ev->y;

			// Calculate the axis of rotation. This is the mouse vector crossed with the Z axis,
			// to give a rotation axis in the XY plane at right-angles to the mouse delta.
			static Vector3 _zAxis(0, 0, 1);
			Vector3 axisRot = deltaPos.crossProduct(_zAxis);

			// Grab the GL widget, and update the modelview matrix with the additional
			// rotation (TODO: may not be the best way to do this).
			if (gtkutil::GLWidget::makeCurrent(widget) != FALSE) {
				// Premultiply the current modelview matrix with the rotation,
				// in order to achieve rotations in eye space rather than object
				// space. At this stage we are only calculating and storing the
				// matrix for the GLDraw callback to use.
				glLoadIdentity();
				glRotatef(-2, axisRot.x(), axisRot.y(), axisRot.z());
				glMultMatrixf(self->_rotation);

				// Save the new GL matrix for GL draw
				glGetFloatv(GL_MODELVIEW_MATRIX, self->_rotation);

				// trigger the GLDraw method to draw the actual model
				gtk_widget_queue_draw(widget);
			}
		}
	}

	void ParticlePreview::callbackGLScroll (GtkWidget* widget, GdkEventScroll* ev, ParticlePreview* self)
	{
		const float inc = 0.1;
		if (ev->direction == GDK_SCROLL_UP)
			self->_camDist += inc;
		else if (ev->direction == GDK_SCROLL_DOWN)
			self->_camDist -= inc;
		gtk_widget_queue_draw(widget);
	}
}
