#ifndef PARTICLEPREVIEW_H_
#define PARTICLEPREVIEW_H_

#include "math/matrix.h"

#include <gtk/gtk.h>
#include <igl.h>
#include <string>

#include "iparticles.h"

#include "gtkutil/glwidget.h"

namespace ui
{
	/** Preview widget for a particle. This class encapsulates the GTK widgets to
	 * render a specified particle using OpenGL, and return a single GTK widget
	 * to be packed into the parent window.
	 */
	class ParticlePreview
	{
			// Top-level widget
			GtkWidget* _widget;

			// GL widget
			gtkutil::GLWidget _glWidget;

			// Current distance between camera and preview
			GLfloat _camDist;

			// Current rotation matrix
			Matrix4 _rotation;

			IParticleDefinition* _particle;

		private:

			/* GTK CALLBACKS */

			static void callbackGLDraw (GtkWidget*, GdkEventExpose*, ParticlePreview*);
			static void callbackGLMotion (GtkWidget*, GdkEventMotion*, ParticlePreview*);
			static void callbackGLScroll (GtkWidget*, GdkEventScroll*, ParticlePreview*);
			static void callbackToggleBBox (GtkToggleToolButton*, ParticlePreview*);

		public:

			/** Construct a ParticlePreview widget.
			 */
			ParticlePreview ();

			/** Destructor for destroying loaded models
			 */
			~ParticlePreview ();

			/** Set the pixel size of the ParticlePreview widget. The widget is always square.
			 *
			 * @param size
			 * The pixel size of the square widget.
			 */
			void setSize (int size);

			/** Initialise the GL preview. This clears the window and sets up the initial
			 * matrices and lights.
			 */
			void initialisePreview ();

			void setParticle (IParticleDefinition* particle);

			IParticleDefinition* getParticle ();

			/** Operator cast to GtkWidget*, for packing into the parent window.
			 */
			operator GtkWidget* ()
			{
				return _widget;
			}
	};
}

#endif /*PARTICLEPREVIEW_H_*/
