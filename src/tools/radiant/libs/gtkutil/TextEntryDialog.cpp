#include "TextEntryDialog.h"
#include <gtk/gtk.h>

namespace gtkutil {

TextEntryDialog::TextEntryDialog (const std::string& title, const std::string& defaultVal, GtkWindow* parent) :
		BlockingTransientWindow(title, parent)
{
	gtk_container_set_border_width(GTK_CONTAINER(getWindow()), 12);
	gtk_window_set_type_hint(GTK_WINDOW(getWindow()), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_default_size(GTK_WINDOW(getWindow()), 350, 75);

	GtkWidget* dialogVBox = gtk_vbox_new(FALSE, 6);
	_widget = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(_widget), defaultVal.c_str());

	gtk_container_add(GTK_CONTAINER(dialogVBox), _widget);

	// Create the close button
	GtkWidget* buttonHBox = gtk_hbox_new(FALSE, 0);
	GtkWidget* okButton = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_box_pack_end(GTK_BOX(buttonHBox), okButton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(okButton), "clicked", G_CALLBACK(callbackClose), this);

	gtk_box_pack_start(GTK_BOX(dialogVBox), buttonHBox, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(getWindow()), dialogVBox);

	show();
}

TextEntryDialog::~TextEntryDialog ()
{
}

void TextEntryDialog::callbackClose (GtkWidget* widget, TextEntryDialog* self)
{
	self->_value = std::string(gtk_entry_get_text(GTK_ENTRY(self->_widget)));
	self->destroy();
}

const std::string& TextEntryDialog::getText () const
{
	return _value;
}

}
