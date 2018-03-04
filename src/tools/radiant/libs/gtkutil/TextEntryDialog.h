#pragma once

#include "gtkutil/window/BlockingTransientWindow.h"
#include <gtk/gtk.h>
#include <string>

namespace gtkutil {

class TextEntryDialog: public BlockingTransientWindow {
private:
	std::string _value;
	GtkWidget* _widget;

	// The callback for the buttons
	static void callbackClose(GtkWidget* widget, TextEntryDialog* self);
public:
	TextEntryDialog (const std::string& title, const std::string& defaultVal, GtkWindow* parent);
	virtual ~TextEntryDialog ();

	const std::string& getText() const;
};

}
