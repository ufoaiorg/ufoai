#ifndef COMMANDLIST_H_
#define COMMANDLIST_H_

#include <string>
#include <iostream>
#include "gtk/gtkwidget.h"
#include "gtk/gtkliststore.h"
#include "gtkutil/window/BlockingTransientWindow.h"

namespace ui {

class CommandListDialog: public gtkutil::BlockingTransientWindow
{
		// The list store containing the list of ColourSchemes
		GtkListStore* _listStore;

	public:
		// Constructor
		CommandListDialog ();

		// This is called to initialise the dialog window / create the widgets
		virtual void populateWindow ();

	private:
		// The callback for the GTK delete-event
		static void callbackClose (GtkWidget* widget, CommandListDialog* self);

}; // class CommandListDialog

} // namespace ui

// -------------------------------------------------------------------------------

// This is the actual command that instantiates the dialog
void ShowCommandListDialog ();

#endif /*COMMANDLIST_H_*/
