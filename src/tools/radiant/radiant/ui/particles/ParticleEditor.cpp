#include "ParticleEditor.h"
#include "iradiant.h"
#include "radiant_i18n.h"

namespace ui
{
	// CONSTANTS

	namespace
	{
		const char* PARTICLEEDITOR_TITLE = _("Particle Editor");
	}

	ParticleEditor::ParticleEditor () :
		_widget(gtk_window_new(GTK_WINDOW_TOPLEVEL))
	{
		// Window properties
		gtk_window_set_transient_for(GTK_WINDOW(_widget), GlobalRadiant().getMainWindow());
		gtk_window_set_modal(GTK_WINDOW(_widget), TRUE);
		gtk_window_set_title(GTK_WINDOW(_widget), PARTICLEEDITOR_TITLE);
		gtk_window_set_position(GTK_WINDOW(_widget), GTK_WIN_POS_CENTER_ON_PARENT);

		// Set the default size of the window
		GdkScreen* scr = gtk_window_get_screen(GTK_WINDOW(_widget));
		gint w = gdk_screen_get_width(scr);
		gint h = gdk_screen_get_height(scr);

		gtk_window_set_default_size(GTK_WINDOW(_widget), gint(w * 0.75), gint(h * 0.8));

		// Create the model preview widget
		gint glSize = gint(h * 0.4);
		_particlePreview.setSize(glSize);

		// Signals
		//g_signal_connect(G_OBJECT(_widget), "delete_event", G_CALLBACK(callbackHide), this);

		// Main window contains a VBox
		GtkWidget* vbx = gtk_vbox_new(FALSE, 3);
		gtk_box_pack_start(GTK_BOX(vbx), createPreviewPanel(), FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(vbx), createButtons(), FALSE, FALSE, 0);
		gtk_container_add(GTK_CONTAINER(_widget), vbx);
	}

	ParticleEditor::~ParticleEditor ()
	{
	}

	// write particle definition to string
	std::string ParticleEditor::save ()
	{
		return "";
	}

	// Show the dialog and enter recursive main loop
	void ParticleEditor::showAndBlock (const std::string& particleID)
	{
		//scripts::IParticlePtr particle = GlobalParticleSystem().getParticle(particleID);

		// Show the dialog
		gtk_widget_show_all(_widget);
		_particlePreview.initialisePreview(); // set up the preview
		//_particlePreview.setParticle(particle);
		gtk_main(); // recursive main loop. This will block until the dialog is closed in some way.
	}

	// Create the buttons panel at bottom of dialog
	GtkWidget* ParticleEditor::createButtons ()
	{
		GtkWidget* hbx = gtk_hbox_new(FALSE, 3);

		GtkWidget* okButton = gtk_button_new_from_stock(GTK_STOCK_OK);
		GtkWidget* cancelButton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

		g_signal_connect(G_OBJECT(okButton), "clicked", G_CALLBACK(callbackOK), this);
		g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(callbackCancel), this);

		gtk_box_pack_end(GTK_BOX(hbx), okButton, FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(hbx), cancelButton, FALSE, FALSE, 0);
		return hbx;
	}

	// Create the preview widget
	GtkWidget* ParticleEditor::createPreviewPanel ()
	{
		return _particlePreview;
	}

	/* GTK CALLBACKS */

	void ParticleEditor::callbackOK (GtkWidget* widget, ParticleEditor* self)
	{
		gtk_main_quit();
		gtk_widget_hide(self->_widget);
	}

	void ParticleEditor::callbackCancel (GtkWidget* widget, ParticleEditor* self)
	{
		gtk_main_quit();
		gtk_widget_hide(self->_widget);
	}
}
