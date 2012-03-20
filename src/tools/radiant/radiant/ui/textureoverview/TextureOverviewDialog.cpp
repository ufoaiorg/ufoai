#include "TextureOverviewDialog.h"
#include "radiant_i18n.h"
#include "iradiant.h"
#include "ieventmanager.h"

#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/TreeModel.h"
#include "../../brush/BrushVisit.h"
#include "../../brush/brushmanip.h"
#include "../findshader/FindShader.h"

#include <map>
#include <string>
#include <gtk/gtkhbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkscrolledwindow.h>

namespace ui
{
	namespace
	{
		const int DLG_DEFAULT_SIZE_X = 600;
		const int DLG_DEFAULT_SIZE_Y = 300;

		enum
		{
			TEXTUREOVERVIEW_IMAGE, TEXTUREOVERVIEW_NAME, TEXTUREOVERVIEW_COUNT, TEXTUREOVERVIEW_SIZE
		};

		class TextureCounter: public BrushInstanceVisitor
		{
		private:

			// ListStore to populate
			GtkListStore* _store;
			typedef std::map<std::string, int> TextureCountMap;
			typedef TextureCountMap::iterator TextureCountMapIter;
			typedef TextureCountMap::const_iterator TextureCountMapConstIter;
			mutable TextureCountMap _map;

			inline
			void addFace (const Face& face) const
			{
				const std::string& shader = face.GetShader();
				TextureCountMapIter i = _map.find(shader);
				int cnt = 0;
				if (i != _map.end()) {
					cnt = i->second;
				}
				_map[shader] = cnt + 1;
			}

		public:
			TextureCounter (GtkListStore* store) :
					_store(store)
			{
			}

			~TextureCounter ()
			{
				for (TextureCountMapConstIter i = _map.begin(); i != _map.end(); ++i) {
					GtkTreeIter iter;
					GdkPixbuf *img = NULL;
					gtk_list_store_append(_store, &iter);
					gtk_list_store_set(_store, &iter, TEXTUREOVERVIEW_IMAGE, img, TEXTUREOVERVIEW_NAME, i->first.c_str(), TEXTUREOVERVIEW_COUNT,
							i->second, -1);
				}
			}

			void operator() (Face& face) const
			{
				addFace(face);
			}

			// BrushInstanceVisitor implementation
			virtual void visit (FaceInstance& face) const
			{
				const Face& f = face.getFace();
				addFace(f);
			}
		};
	}

	TextureOverviewDialog::TextureOverviewDialog () :
			gtkutil::PersistentTransientWindow(_("Texture overview"), GlobalRadiant().getMainWindow()), _store(
					gtk_list_store_new(TEXTUREOVERVIEW_SIZE, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT))
	{
		gtk_window_set_default_size(GTK_WINDOW(getWindow()), DLG_DEFAULT_SIZE_X, DLG_DEFAULT_SIZE_Y);
		gtk_container_set_border_width(GTK_CONTAINER(getWindow()), 12);
		gtk_window_set_type_hint(GTK_WINDOW(getWindow()), GDK_WINDOW_TYPE_HINT_DIALOG);

		// Create all the widgets
		populateWindow();

		// Propagate shortcuts to the main window
		GlobalEventManager().connectDialogWindow(GTK_WINDOW(getWindow()));

		// Show the window and its children
		show();
	}

	TextureOverviewDialog::~TextureOverviewDialog ()
	{
		// Propagate shortcuts to the main window
		GlobalEventManager().disconnectDialogWindow(GTK_WINDOW(getWindow()));
	}

	void TextureOverviewDialog::populateWindow ()
	{
		GtkWidget* dialogVBox = gtk_vbox_new(FALSE, 6);

		// Set up the info table
		GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_store));
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);

		// Connect up selection changed callback
		_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
		g_signal_connect(G_OBJECT(_selection), "changed", G_CALLBACK(onSelectionChanged), this);

		GtkCellRenderer* rend;
		GtkTreeViewColumn* col;

		rend = gtk_cell_renderer_pixbuf_new();
		gtk_cell_renderer_set_fixed_size(rend, 128, -1);
		col = gtk_tree_view_column_new();
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
		gtk_tree_view_column_set_title(col, _("Image"));
		gtk_tree_view_column_pack_start(col, rend, false);
		gtk_tree_view_column_add_attribute(col, rend, "pixbuf", TEXTUREOVERVIEW_IMAGE);

		rend = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(_("Name"), rend, "text", TEXTUREOVERVIEW_NAME, NULL);
		g_object_set(G_OBJECT(rend), "weight", 400, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

		rend = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(_("Count"), rend, "text", TEXTUREOVERVIEW_COUNT, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

		GtkWidget *scroll = gtkutil::ScrolledFrame(view);
		gtk_box_pack_start(GTK_BOX(dialogVBox), GTK_WIDGET(scroll), TRUE, TRUE, 0);

		GtkWidget* hbox = gtk_hbox_new(FALSE, 6);
		GtkWidget* closeButton = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
		gtk_widget_set_size_request(closeButton, -1, -1);
		g_signal_connect(G_OBJECT(closeButton), "clicked", G_CALLBACK(onClose), this);
		gtk_box_pack_end(GTK_BOX(hbox), closeButton, FALSE, FALSE, 0);
		GtkWidget* findAndReplaceButton = gtk_button_new_from_stock(GTK_STOCK_FIND_AND_REPLACE);
		gtk_widget_set_size_request(findAndReplaceButton, -1, -1);
		g_signal_connect(G_OBJECT(findAndReplaceButton), "clicked", G_CALLBACK(onFindAndReplace), this);
		gtk_box_pack_end(GTK_BOX(hbox), findAndReplaceButton, FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(dialogVBox), GTK_WIDGET(hbox), FALSE, FALSE, 0);

		gtk_container_add(GTK_CONTAINER(getWindow()), dialogVBox);

		TextureCounter counter(_store);
		Scene_ForEachBrush_ForEachFace(GlobalSceneGraph(), counter);
	}

	void TextureOverviewDialog::onClose (GtkWidget* widget, TextureOverviewDialog* self)
	{
		// Call the DialogWindow::destroy method and remove self from heap
		self->destroy();
	}

	void TextureOverviewDialog::onFindAndReplace (GtkWidget* widget, TextureOverviewDialog* self)
	{
		ui::FindAndReplaceShader(self->_selectedTexture);
	}

	void TextureOverviewDialog::onSelectionChanged (GtkWidget* widget, TextureOverviewDialog* self)
	{
		self->_selectedTexture = gtkutil::TreeModel::getSelectedString(self->_selection, TEXTUREOVERVIEW_NAME);
		GlobalSelectionSystem().setSelectedAllComponents(false);
		GlobalSelectionSystem().setSelectedAll(false);
		if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent) {
			Scene_BrushSelectByShader_Component(GlobalSceneGraph(), self->_selectedTexture);
		} else {
			Scene_BrushSelectByShader(GlobalSceneGraph(), self->_selectedTexture);
		}
	}
}
