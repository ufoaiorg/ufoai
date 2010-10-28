#include "UMPDefinitionView.h"

#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include "gtkutil/LeftAlignedLabel.h"
#include "gtkutil/dialog.h"
#include "ifilesystem.h"
#include "radiant_i18n.h"
#include "AutoPtr.h"
#include "stream/textfilestream.h"
#include "iarchive.h"

namespace ui
{
	UMPDefinitionView::UMPDefinitionView () :
		_vbox(gtk_vbox_new(FALSE, 6)), _view("ump", false)
	{
		GtkTable* table = GTK_TABLE(gtk_table_new(2, 2, FALSE));
		gtk_box_pack_start(GTK_BOX(_vbox), GTK_WIDGET(table), FALSE, FALSE, 0);

		GtkWidget* nameLabel = gtkutil::LeftAlignedLabel(_("UMP:"));

		_umpFileName = gtkutil::LeftAlignedLabel("");

		gtk_widget_set_size_request(nameLabel, 90, -1);

		gtk_table_attach(table, nameLabel, 0, 1, 0, 1, (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);

		gtk_table_attach_defaults(table, _umpFileName, 1, 2, 0, 1);

		gtk_box_pack_start(GTK_BOX(_vbox), gtkutil::LeftAlignedLabel(_("Definition")), FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(_vbox), _view.getWidget(), TRUE, TRUE, 0);
	}

	void UMPDefinitionView::setUMPFile (const std::string& umpFile)
	{
		_umpFile = umpFile;

		update();
	}

	GtkWidget* UMPDefinitionView::getWidget ()
	{
		return _vbox;
	}

	void UMPDefinitionView::update ()
	{
		// Find the material
		AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(_umpFile));
		if (!file) {
			// Null-ify the contents
			gtk_label_set_markup(GTK_LABEL(_umpFileName), "");

			gtk_widget_set_sensitive(_view.getWidget(), FALSE);

			return;
		}

		// Add the ump file name
		gtk_label_set_markup(GTK_LABEL(_umpFileName), ("<b>" + _umpFile + "</b>").c_str());

		gtk_widget_set_sensitive(_view.getWidget(), TRUE);

		_view.setContents(file->getString());
	}

	void UMPDefinitionView::append (const std::string& append)
	{
		if (append.length()) {
			const std::string& content = _view.getContents();
			_view.setContents(content + append);
		}
	}

	void UMPDefinitionView::save ()
	{
		const std::string& content = _view.getContents();
		TextFileOutputStream out(GlobalRadiant().getGamePath() + _umpFile);
		if (out.failed()) {
			g_message("Error saving file to '%s'.", _umpFile.c_str());
			gtkutil::errorDialog(_("Error saving ump file"));
			return;
		}
		out << content.c_str();
	}

} // namespace ui
