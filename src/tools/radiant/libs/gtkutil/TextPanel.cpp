#include "TextPanel.h"

#include <gtk/gtkentry.h>

namespace gtkutil
{
	// Constructor
	TextPanel::TextPanel ()
	{
		_widget = gtk_entry_new();
	}

	// Destructor
	TextPanel::~TextPanel ()
	{
		if (GTK_IS_WIDGET(_widget))
			gtk_widget_destroy(_widget);
	}

	// Show and return the widget
	GtkWidget* TextPanel::_getWidget () const
	{
		return _widget;
	}

	// Set the displayed value
	void TextPanel::setValue (const std::string& value)
	{
		gtk_entry_set_text(GTK_ENTRY(_widget), value.c_str());
	}

	// Get the edited value
	std::string TextPanel::getValue () const
	{
		return std::string(gtk_entry_get_text(GTK_ENTRY(_widget)));
	}
} // namespace gtkutil
