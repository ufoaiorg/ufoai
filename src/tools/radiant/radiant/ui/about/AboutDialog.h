#ifndef ABOUTDIALOG_H_
#define ABOUTDIALOG_H_

#include <string>
#include "gtk/gtk.h"
#include "gtkutil/window/BlockingTransientWindow.h"

/** greebo: The AboutDialog displays information about DarkRadiant and
 * 			the subsystems OpenGL and GTK+
 *
 * Note: Show the dialog by instantiating this class with NEW on the heap,
 * as it's deriving from gtkutil::DialogWindow. It destroys itself upon dialog closure
 * and frees the allocated memory.
 */
namespace ui {

class AboutDialog :
	public gtkutil::BlockingTransientWindow
{
public:
	// Constructor
	AboutDialog();

	/** greebo: Shows the dialog (allocates on heap, dialog self-destructs)
	 */
	static void showDialog();

private:
	// This is called to initialise the dialog window / create the widgets
	void populateWindow();

	// The callback for the buttons
	static void callbackClose(GtkWidget* widget, AboutDialog* self);
}; // class AboutDialog

} // namespace ui

#endif /*ABOUTDIALOG_H_*/
