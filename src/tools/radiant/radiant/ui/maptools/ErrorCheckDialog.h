#ifndef ERRORCHECKDIALOG_H_
#define ERRORCHECKDIALOG_H_

#include <string>
#include <gtk/gtkwidget.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeselection.h>
#include "gtkutil/window/PersistentTransientWindow.h"

namespace ui {

/* The dialog providing the map error check functionality.
 *
 * Note: Show the dialog by instantiating it. It automatically enters a
 *       GTK main loop after show().
 */
class ErrorCheckDialog: public gtkutil::PersistentTransientWindow
{
	private:

		// List store containing error messages and details
		GtkListStore* _listStore;

		// The listview for the above store
		GtkWidget* _listView;

	public:
		ErrorCheckDialog ();

		~ErrorCheckDialog ();

		// This is called to initialise the dialog window / create the widgets
		virtual void populateWindow ();

		// Shows the dialog (allocates on heap, dialog self-destructs)
		static void showDialog ();

	private:

		GtkWidget* createButtons ();
		GtkWidget* createTreeView ();

		/* GTK CALLBACKS */
		static void callbackClose (GtkWidget*, ErrorCheckDialog*);
		static void callbackFix (GtkWidget*, ErrorCheckDialog*);
		static void callbackSelect (GtkTreeSelection* sel, ErrorCheckDialog* self);
};

}

#endif
