#include "MaterialDefinitionView.h"

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
	MaterialDefinitionView::MaterialDefinitionView () :
		_vbox(gtk_vbox_new(FALSE, 6)), _view("material", false)
	{
		GtkTable* table = GTK_TABLE(gtk_table_new(2, 2, FALSE));
		gtk_box_pack_start(GTK_BOX(_vbox), GTK_WIDGET(table), FALSE, FALSE, 0);

		GtkWidget* nameLabel = gtkutil::LeftAlignedLabel(_("Material:"));
		GtkWidget* materialFileLabel = gtkutil::LeftAlignedLabel(_("Defined in:"));

		_materialName = gtkutil::LeftAlignedLabel("");
		_filename = gtkutil::LeftAlignedLabel("");

		gtk_widget_set_size_request(nameLabel, 90, -1);
		gtk_widget_set_size_request(materialFileLabel, 90, -1);

		gtk_table_attach_defaults(table, nameLabel, 0, 1, 0, 1);
		gtk_table_attach_defaults(table, materialFileLabel, 0, 1, 1, 2);

		gtk_table_attach_defaults(table, _materialName, 1, 2, 0, 1);
		gtk_table_attach_defaults(table, _filename, 1, 2, 1, 2);

		gtk_box_pack_start(GTK_BOX(_vbox), gtkutil::LeftAlignedLabel(_("Definition")), FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(_vbox), _view.getWidget(), TRUE, TRUE, 0);
	}

	void MaterialDefinitionView::setMaterial (const std::string& material)
	{
		_material = material;

		update();
	}

	GtkWidget* MaterialDefinitionView::getWidget ()
	{
		return _vbox;
	}

	void MaterialDefinitionView::update ()
	{
		// Find the material
		AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(_material));
		if (!file) {
			// Null-ify the contents
			gtk_label_set_markup(GTK_LABEL(_materialName), "");
			gtk_label_set_markup(GTK_LABEL(_filename), "");

			gtk_widget_set_sensitive(_view.getWidget(), FALSE);

			return;
		}

		// Add the material and file name
		gtk_label_set_markup(GTK_LABEL(_materialName), ("<b>" + _material + "</b>").c_str());
		gtk_label_set_markup(GTK_LABEL(_filename), ("<b>" + _material + "</b>").c_str());

		gtk_widget_set_sensitive(_view.getWidget(), TRUE);

		_view.setContents(file->getString());
	}

	void MaterialDefinitionView::append (const std::string& append)
	{
		if (append.length()) {
			const std::string& content = _view.getContents();
			if (content.find(append) == std::string::npos)
				_view.setContents(content + append);
		}
	}

	void MaterialDefinitionView::save ()
	{
		const std::string& content = _view.getContents();
		const std::string& gamePath = GlobalRadiant().getFullGamePath();
		std::string fullpath = gamePath + "/" + _material;
		TextFileOutputStream out(fullpath);
		if (out.failed()) {
			g_message("Error saving file to '%s'.", fullpath.c_str());
			gtkutil::errorDialog(_("Error saving material file"));
			return;
		}
		out << content;
	}

} // namespace ui
