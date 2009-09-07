/**
 * @file particlebrowser.h
 */

/*
 Copyright (C) 1997-2008 UFO:AI Team

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

#ifndef PARTICLEBROWSER_H_
#define PARTICLEBROWSER_H_

typedef struct _GtkWidget GtkWidget;

GtkWidget* ParticleBrowser_constructNotebookTab ();

void ParticleBrowser_Construct (void);
void ParticleBrowser_Destroy (void);

#include "../ui/common/TexturePreviewCombo.h"

#include <gtk/gtkwidget.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtktreeselection.h>

namespace ui
{
	class ParticleBrowser
	{
			// Main widget
			GtkWidget* _widget;

			// Main tree store, view and selection
			GtkTreeStore* _treeStore;
			GtkWidget* _treeView;
			GtkTreeSelection* _selection;

			// Texture preview combo (GL widget and info table)
			TexturePreviewCombo _preview;

		private:

			/* GTK CALLBACKS */

			static gboolean _onExpose (GtkWidget*, GdkEventExpose*, ParticleBrowser*);
			static bool _onRightClick (GtkWidget*, GdkEventButton*, ParticleBrowser*);
			static void _onSelectionChanged (GtkWidget*, ParticleBrowser*);

			/* Tree selection query functions */

			std::string getSelectedName (); // return name of selection

		public:

			/** Return the singleton instance.
			 */
			static ParticleBrowser& getInstance() {
				static ParticleBrowser _instance;
				return _instance;
			}

			/** Return the main widget for packing into
			 * the groupdialog or other parent container.
			 */
			GtkWidget* getWidget ()
			{
				gtk_widget_show_all(_widget);
				return _widget;
			}

			/** Constructor creates GTK widgets.
			 */
			ParticleBrowser ();
	};
}

#endif
