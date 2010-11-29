#include "AboutDialog.h"

#include "radiant_i18n.h"
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include "igl.h"
#include "iregistry.h"
#include "iradiant.h"
#include "iuimanager.h"
#include "version.h"
#include "string/string.h"
#include "gtkutil/LeftAlignedLabel.h"
#include "gtkutil/LeftAlignment.h"
#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/image.h"

#include "../../sound/OpenAL.h"

namespace ui {

	namespace {
		const std::string RKEY_SHOW_BUILD_TIME = "user/showBuildTime";
		const std::string WINDOW_TITLE = _("About UFORadiant");
	}

AboutDialog::AboutDialog() :
	gtkutil::BlockingTransientWindow(WINDOW_TITLE, GlobalRadiant().getMainWindow())
{
	gtk_container_set_border_width(GTK_CONTAINER(getWindow()), 12);
	gtk_window_set_type_hint(GTK_WINDOW(getWindow()), GDK_WINDOW_TYPE_HINT_DIALOG);

	// Create all the widgets
	populateWindow();
}

void AboutDialog::populateWindow() {
	GtkWidget* dialogVBox = gtk_vbox_new(FALSE, 6);

	GtkWidget* topHBox = gtk_hbox_new(FALSE, 12);

	GtkWidget* image = gtk_image_new_from_pixbuf(
		gtkutil::getLocalPixbuf("logo.png")
	);
	gtk_box_pack_start(GTK_BOX(topHBox), image, FALSE, FALSE, 0);

	std::string date = __DATE__;
	std::string time = __TIME__;

	bool showBuildTime = GlobalRegistry().get(RKEY_SHOW_BUILD_TIME) == "1";
	std::string buildDate = (showBuildTime) ? date + " " + time : date;

	std::string appName("UFORadiant " RADIANT_VERSION);

	std::string appNameStr =
		string::format(_("<b><span size=\"large\">%s</span></b>"), appName.c_str()) + "\n";

	std::string buildDateStr =
		string::format(_("Build date: %s"), buildDate.c_str()) + "\n\n";

	std::string descStr = _("<b>UFO:AI</b> (ufoai.sourceforge.net)\n\n"
		"This product contains software technology\n"
		"from id Software, Inc. ('id Technology').\n"
		"id Technology 2000 id Software,Inc.\n\n"
		"UFORadiant is based on the GPL version\n"
		"of GtkRadiant (www.qeradiant.com) and\n"
		"DarkRadiant (darkradiant.sf.net)\n\n");

	GtkWidget* title = gtkutil::LeftAlignedLabel(appNameStr + buildDateStr + descStr);

	GtkWidget* alignment = gtk_alignment_new(0.0f, 0.0f, 1.0f, 0.0f);
	gtk_container_add(GTK_CONTAINER(alignment), title);
	gtk_box_pack_start(GTK_BOX(topHBox), alignment, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(dialogVBox), topHBox, FALSE, FALSE, 0);

	// GTK Version
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignedLabel(
		_("<b>GTK+ Properties</b>")), FALSE, FALSE, 0);

	GtkWidget* gtkVersion = gtkutil::LeftAlignedLabel(
		string::format(_("Version: %d.%d.%d"), gtk_major_version, gtk_minor_version, gtk_micro_version)
	);
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignment(gtkVersion, 18), FALSE, FALSE, 0);

	// Glib Version
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignedLabel(
		_("<b>glib Properties</b>")), FALSE, FALSE, 0);

	GtkWidget* glibVersion = gtkutil::LeftAlignedLabel(
		string::format(_("Version: %d.%d.%d"), glib_major_version, glib_minor_version, glib_micro_version)
	);
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignment(glibVersion, 18), FALSE, FALSE, 0);

	// GTKGLExt Version
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignedLabel(
		_("<b>GTKGLExt Properties</b>")), FALSE, FALSE, 0);

	GtkWidget* gtkGlExtVersion = gtkutil::LeftAlignedLabel(
		string::format(_("Version: %d.%d.%d"), gtkglext_major_version, gtkglext_minor_version, gtkglext_micro_version)
	);
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignment(gtkGlExtVersion, 18), FALSE, FALSE, 0);

	// OpenGL
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignedLabel(
		_("<b>OpenGL Properties</b>")), FALSE, FALSE, 0);

	// If anybody knows a better method to convert glubyte* to char*, please tell me...
	std::string glVendorStr = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
	std::string glVersionStr = reinterpret_cast<const char*>(glGetString(GL_VERSION));
	std::string glRendererStr = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

	GtkWidget* glVendor = gtkutil::LeftAlignedLabel(string::format(_("Vendor: %s"), glVendorStr.c_str()));
	GtkWidget* glVersion = gtkutil::LeftAlignedLabel(string::format(_("Version: %s"), glVersionStr.c_str()));
	GtkWidget* glRenderer = gtkutil::LeftAlignedLabel(string::format(_("Renderer: %s"), glRendererStr.c_str()));

	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignment(glVendor, 18), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignment(glVersion, 18), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignment(glRenderer, 18), FALSE, FALSE, 0);

	// OpenGL extensions
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignedLabel(
		_("<b>OpenGL Extensions</b>")), FALSE, FALSE, 0);

	GtkWidget* glExtTextView = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(glExtTextView), FALSE);
	gtk_text_buffer_set_text(
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(glExtTextView)),
		reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)),
		-1
	);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(glExtTextView), GTK_WRAP_WORD);

	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignment(
		gtkutil::ScrolledFrame(glExtTextView), 18, 1.0f),
		TRUE, TRUE, 0
	);

	// OpenAL
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignedLabel(
		_("<b>OpenAL Properties</b>")), FALSE, FALSE, 0);

	const char* alVendorStr = reinterpret_cast<const char*>(alGetString(AL_VENDOR));
	const char* alVersionStr = reinterpret_cast<const char*>(alGetString(AL_VERSION));
	const char* alRendererStr = reinterpret_cast<const char*>(alGetString(AL_RENDERER));

	GtkWidget* alVendor = gtkutil::LeftAlignedLabel(string::format(_("Vendor: %s"), alVendorStr ? alVendorStr : ""));
	GtkWidget* alVersion = gtkutil::LeftAlignedLabel(string::format(_("Version: %s"), alVersionStr ? alVersionStr : ""));
	GtkWidget* alRenderer = gtkutil::LeftAlignedLabel(string::format(_("Renderer: %s"), alRendererStr ? alRendererStr : ""));

	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignment(alVendor, 18), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignment(alVersion, 18), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignment(alRenderer, 18), FALSE, FALSE, 0);

	// OpenGL extensions
	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignedLabel(
		_("<b>OpenAL Extensions</b>")), FALSE, FALSE, 0);

	GtkWidget* alExtTextView = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(alExtTextView), FALSE);
	const char* alExtensions = reinterpret_cast<const char*>(alGetString(AL_EXTENSIONS));
	gtk_text_buffer_set_text(
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(alExtTextView)),
		alExtensions ? alExtensions : "",
		-1
	);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(alExtTextView), GTK_WRAP_WORD);

	gtk_box_pack_start(GTK_BOX(dialogVBox), gtkutil::LeftAlignment(
		gtkutil::ScrolledFrame(alExtTextView), 18, 1.0f),
		TRUE, TRUE, 0
	);

	// Create the close button
	GtkWidget* buttonHBox = gtk_hbox_new(FALSE, 0);
	GtkWidget* okButton = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_box_pack_end(GTK_BOX(buttonHBox), okButton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(okButton), "clicked", G_CALLBACK(callbackClose), this);

	gtk_box_pack_start(GTK_BOX(dialogVBox), buttonHBox, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(getWindow()), dialogVBox);
}

void AboutDialog::callbackClose(GtkWidget* widget, AboutDialog* self) {
	self->destroy();
}

void AboutDialog::showDialog() {
	AboutDialog dialog;
	dialog.show(); // blocks
}

} // namespace ui
