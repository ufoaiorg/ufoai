#include "SourceView.h"

#include "radiant_i18n.h"
#include "itextstream.h"
#include "iregistry.h"

#include "../../radiant/environment.h"

#include "ScrolledFrame.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gtksourceview/gtksourcelanguagemanager.h>
#include <gtksourceview/gtksourcestyleschememanager.h>
#include <gtksourceview/gtksourceview.h>

#include "dialog.h"

namespace gtkutil
{
	inline gboolean escape_clear_focus_widget (GtkWidget* widget, GdkEventKey* event, gpointer data)
	{
		if (event->keyval == GDK_Escape) {
			gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(widget))), NULL);
			return TRUE;
		}
		return FALSE;
	}

	SourceView::SourceView (const std::string& language, bool readOnly)
	{
		// Set the search path to the language and style files
		gchar* directories[2];

		std::string langFilesDir = GlobalRegistry().get(RKEY_APP_PATH) + "sourceviewer/";

		directories[0] = const_cast<gchar*> (langFilesDir.c_str()); // stupid GtkSourceLanguageManager is expecting non-const gchar* pointer...
		directories[1] = NULL;

		GtkSourceStyleSchemeManager* styleSchemeManager = gtk_source_style_scheme_manager_get_default();
		gtk_source_style_scheme_manager_set_search_path(styleSchemeManager, directories);
		gtk_source_style_scheme_manager_force_rescan(styleSchemeManager);

		_langManager = gtk_source_language_manager_new();
		gtk_source_language_manager_set_search_path(_langManager, directories);

		GtkSourceLanguage* lang = gtk_source_language_manager_get_language(_langManager, language.c_str());

		if (lang == NULL) {
			gtkutil::errorDialog(_("Cannot find language for source viewer"));
			g_error("SourceView: Cannot find language %s\n", language.c_str());
			throw std::runtime_error("SourceView: Cannot find language");
		}

		// Remember the pointers to the textbuffers
		_buffer = gtk_source_buffer_new_with_language(lang);
		gtk_source_buffer_set_highlight_syntax(_buffer, TRUE);

		_view = GTK_SOURCE_VIEW(gtk_source_view_new_with_buffer(_buffer));

		gtk_widget_set_size_request(GTK_WIDGET(_view), 0, -1); // allow shrinking
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(_view), GTK_WRAP_NONE);
		gtk_text_view_set_editable(GTK_TEXT_VIEW(_view), readOnly ? FALSE : TRUE);

		gtk_source_view_set_show_line_numbers(_view, TRUE);
		gtk_source_view_set_auto_indent(_view, TRUE);

		// Use a fixed width font
		PangoFontDescription* fontDesc = pango_font_description_from_string("Monospace");

		if (fontDesc != NULL) {
			gtk_widget_modify_font(GTK_WIDGET(_view), fontDesc);
		}

		// Use a tab size of 4
		gtk_source_view_set_tab_width(GTK_SOURCE_VIEW(_view), 4);

		g_signal_connect(G_OBJECT(_view), "key_press_event", G_CALLBACK(escape_clear_focus_widget), 0);

		_widget = gtkutil::ScrolledFrame(GTK_WIDGET(_view));
	}

	SourceView::~SourceView ()
	{
		g_object_unref(_langManager);
	}

	void SourceView::setContents (const std::string& newContents)
	{
		gtk_text_buffer_set_text(GTK_TEXT_BUFFER(_buffer), newContents.c_str(), -1);
	}

	void SourceView::goToLine (int lineNumber)
	{
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_line(GTK_TEXT_BUFFER(_buffer), &iter, lineNumber);
		gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(_buffer), &iter);
		gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(_view), gtk_text_buffer_get_insert(GTK_TEXT_BUFFER(_buffer)));
	}

	std::string SourceView::getContents ()
	{
		GtkTextIter start;
		GtkTextIter end;

		gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(_buffer), &start, &end);

		// Extract the script from the input window
		gchar* text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(_buffer), &start, &end, TRUE);

		// Convert to std::string, free the GLIB stuff and return
		std::string contents(text);
		g_free(text);

		return contents;
	}

	void SourceView::clear ()
	{
		gtk_text_buffer_set_text(GTK_TEXT_BUFFER(_buffer), "", -1);
	}

} // namespace gtkutil
