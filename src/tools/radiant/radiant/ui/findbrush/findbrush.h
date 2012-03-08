/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef FINDBRUSH_H
#define FINDBRUSH_H

#include <gtk/gtkwidget.h>
#include "gtkutil/window/PersistentTransientWindow.h"

namespace ui {

class FindBrushDialog: public gtkutil::PersistentTransientWindow
{
		// The entry fields
		GtkWidget* _entityEntry;
		GtkWidget* _brushEntry;

	public:
		FindBrushDialog ();
		~FindBrushDialog ();

	private:

		// This is called to initialise the dialog window / create the widgets
		void populateWindow ();

		// Helper method to create the OK/Cancel button
		GtkWidget* createButtons ();

		// The callback for the buttons
		static void onFind(GtkWidget* widget, FindBrushDialog* self);
		static void onClose(GtkWidget* widget, FindBrushDialog* self);
};

}

#endif
