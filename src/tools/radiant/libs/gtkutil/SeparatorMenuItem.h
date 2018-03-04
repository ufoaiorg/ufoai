#pragma once

#include <gtk/gtk.h>
#include <string>

namespace gtkutil
{

/** Encapsulation of a GtkSeparatorMenuItem for use in a GtkMenu.
 */

class SeparatorMenuItem
{
public:

	// Constructor
	SeparatorMenuItem()
	{}

	// Operator cast to GtkWidget* for packing into a menu
	operator GtkWidget* () {
		GtkWidget* separatorMenuItem = gtk_separator_menu_item_new();
		return separatorMenuItem;
	}
};

}
