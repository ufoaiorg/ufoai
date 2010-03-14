#ifndef PARTICLEEDITOR_H_
#define PARTICLEEDITOR_H_

#include <gtk/gtk.h>
#include <string>
#include <cmath>
#include "igl.h"
#include "ParticlePreview.h"

namespace ui
{
	class ParticleEditor
	{
		private:

			// Main dialog widget
			GtkWidget* _widget;

			// Model preview widget
			ParticlePreview _particlePreview;

			// Show the dialog and block
			void showAndBlock ();

			GtkWidget* createButtons ();
			GtkWidget* createPreviewPanel ();

			// Initialise the GL widget, to avoid doing this every frame
			void initialisePreview ();

			/* GTK CALLBACKS */

			static void callbackOK (GtkWidget*, ParticleEditor*);
			static void callbackCancel (GtkWidget*, ParticleEditor*);

		public:

			ParticleEditor ();

			virtual ~ParticleEditor ();

			std::string save ();

			void showAndBlock (const std::string& particleID);
	};
}

#endif /* PARTICLEEDITOR_H_ */
