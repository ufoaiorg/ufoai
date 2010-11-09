#ifndef PARTICLESELECTOR_H_
#define PARTICLESELECTOR_H_

#include <gtk/gtkwidget.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeselection.h>

#include "ParticlePreview.h"

#include <string>

namespace ui
{

	/**
	 * Dialog for listing and selection of sound shaders.
	 */
	class ParticleSelector
	{
			// Main dialog widget
			GtkWidget* _widget;

			// Tree store for shaders, and the tree selection
			GtkTreeStore* _treeStore;
			GtkTreeSelection* _treeSelection;

			ParticlePreview _preview;

			// Last selected soundfile
			std::string _selectedSound;

		private:

			// Widget construction
			GtkWidget* createTreeView ();
			GtkWidget* createButtons ();

			/* GTK CALLBACKS */
			static gboolean _onDelete (GtkWidget* w, GdkEvent* e, ParticleSelector* self);
			static void _onOK (GtkWidget*, ParticleSelector*);
			static void _onCancel (GtkWidget*, ParticleSelector*);
			static void _onSelectionChange (GtkTreeSelection*, ParticleSelector*);

		public:

			/**
			 * Constructor creates widgets.
			 *
			 * The parent window.
			 */
			ParticleSelector ();

			/**
			 * Display the dialog and return the selection.
			 */
			std::string chooseParticle ();

	};

}

#endif /*PARTICLESELECTOR_H_*/
