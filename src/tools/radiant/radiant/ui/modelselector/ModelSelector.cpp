#include "ModelSelector.h"

#include "radiant_i18n.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/image.h"
#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/ModalProgressDialog.h"
#include "gtkutil/TreeModel.h"

#include "ifilesystem.h"
#include "iradiant.h"
#include "iregistry.h"
#include "os/path.h"
#include "generic/callback.h"
#include "../../referencecache/referencecache.h"
#include "../Icons.h"

#include <cmath>
#include <iostream>
#include <vector>
#include <sstream>

namespace ui
{
	// CONSTANTS

	namespace
	{
		const char* MODELSELECTOR_TITLE = _("Choose model");
		const std::string MODELS_FOLDER = "models";
		const char* MD2_EXTENSION = "md2";
		const char* MD3_EXTENSION = "md3";
		const char* OBJ_EXTENSION = "obj";

		// Treestore enum
		enum
		{
			NAME_COLUMN, // e.g. "chair1.md2"
			DIRNAME_COLUMN, // e.g. models/objects/
			FULLNAME_COLUMN, // e.g. "models/objects/chair1.md2"
			SKIN_COLUMN, // e.e. "chair1_brown_wood", or "" for no skin
			SKIN_INDEX,
			IMAGE_COLUMN, // icon to display
			N_COLUMNS
		};
	}

	// Constructor.

	ModelSelector::ModelSelector () :
		_widget(gtk_window_new(GTK_WINDOW_TOPLEVEL)), _treeStore(gtk_tree_store_new(N_COLUMNS, G_TYPE_STRING,
				G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, GDK_TYPE_PIXBUF)), _infoStore(gtk_list_store_new(2,
				G_TYPE_STRING, G_TYPE_STRING)), _lastModel(""), _lastSkin(-1)
	{
		// Window properties
		gtk_window_set_transient_for(GTK_WINDOW(_widget), GlobalRadiant().getMainWindow());
		gtk_window_set_modal(GTK_WINDOW(_widget), TRUE);
		gtk_window_set_title(GTK_WINDOW(_widget), MODELSELECTOR_TITLE);
		gtk_window_set_position(GTK_WINDOW(_widget), GTK_WIN_POS_CENTER_ON_PARENT);

		// Set the default size of the window
		GtkWindow* mainWindow = GlobalRadiant().getMainWindow();
		gint w;
		gint h;
		gtk_window_get_size(mainWindow,&w,&h);

		gtk_window_set_default_size(GTK_WINDOW(_widget), gint(w * 0.75), gint(h * 0.8));

		// Create the model preview widget
		float previewHeightFactor = GlobalRegistry().getFloat("user/ui/ModelSelector/previewSizeFactor");
		gint glSize = gint(h * previewHeightFactor);
		_modelPreview.setSize(glSize);

		// Signals
		g_signal_connect(G_OBJECT(_widget), "delete_event", G_CALLBACK(callbackHide), this);

		// Main window contains a VBox
		GtkWidget* vbx = gtk_vbox_new(FALSE, 3);
		gtk_box_pack_start(GTK_BOX(vbx), createTreeView(), TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(vbx), createPreviewAndInfoPanel(), FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(vbx), createButtons(), FALSE, FALSE, 0);
		gtk_container_add(GTK_CONTAINER(_widget), vbx);
	}

	// Show the dialog and enter recursive main loop
	ModelAndSkin ModelSelector::showAndBlock ()
	{
		// Show the dialog
		gtk_widget_show_all(_widget);
		_modelPreview.initialisePreview(); // set up the preview
		gtk_main(); // recursive main loop. This will block until the dialog is closed in some way.

		// Reset the preview model to release resources
		_modelPreview.setModel("");

		// Construct the model/skin combo and return it
		return ModelAndSkin(_lastModel, _lastSkin);
	}

	// Static function to display the instance, and return the selected
	// model to the calling function

	ModelAndSkin ModelSelector::chooseModel ()
	{
		static ModelSelector _selector;
		return _selector.showAndBlock();
	}

	// File-local functor object to retrieve model names from global VFS

	namespace
	{
		struct ModelFileFunctor
		{
				typedef const std::string& first_argument_type;

				bool _scanForDirectory;

				// Tree store to populate
				GtkTreeStore* _store;

				const gtkutil::ModalProgressDialog& _dialog;

				// the path we are currently loading
				const std::string _path;

				DirIterMap& _dirIterMap;

				// Constructor

				ModelFileFunctor (GtkTreeStore* store, const gtkutil::ModalProgressDialog& dialog, const std::string& path, DirIterMap& dirIterMap) :
					_store(store), _dialog(dialog), _path(path), _dirIterMap(dirIterMap)
				{
				}

				void setDirectory(bool scanForDirectory) {
					_scanForDirectory  = scanForDirectory;
				}

				// Recursive function to add a given model path ("models/something/model.ext")
				// to its correct place in the tree. This is done by maintaining a cache of
				// directory nodes ("models/something", "models/") against
				// GtkIters that point to the corresponding row in the tree model. On each call,
				// the parent node is recursively calculated, and the node provided as an argument
				// added as a child.

				void addDirectory (const std::string& dirName, GtkTreeIter* parIter)
				{
					std::string relativePath = DirectoryCleaned(_path + dirName);

					DirIterMap::iterator it = _dirIterMap.find(relativePath);
					if (it != _dirIterMap.end())
						return;

					// Add the fields to the treeview
					GtkTreeIter iter;
					gtk_tree_store_append(_store, &iter, parIter);
					gtk_tree_store_set(_store, &iter, NAME_COLUMN, dirName.c_str(), DIRNAME_COLUMN,
							relativePath.c_str(), FULLNAME_COLUMN, "", SKIN_COLUMN, "", IMAGE_COLUMN,
							gtkutil::getLocalPixbuf(ui::icons::ICON_FOLDER), -1);
					GtkTreeIter* dynIter = gtk_tree_iter_copy(&iter); // get a heap-allocated iter

					// Now add a map entry that maps our directory name to the row we just
					// added
					_dirIterMap[relativePath] = dynIter;
				}

				void addModel (const std::string& fileName, GtkTreeIter* parIter)
				{
					// Decide which image to use, based on the file extension (or the folder
					// image if there is no extension). Also, set a flag indicating that we
					// have an actual model rather than a directory, so that the fullname
					// tree column can be populated

					std::string imgPath("");

					if (os::getExtension(fileName) == MD3_EXTENSION) {
						imgPath = ui::icons::ICON_MD3;
					} else if (os::getExtension(fileName) == MD2_EXTENSION) {
						imgPath = ui::icons::ICON_MD2;
					} else if (os::getExtension(fileName) == OBJ_EXTENSION) {
						imgPath = ui::icons::ICON_OBJ;
					}

					if (imgPath.empty())
						return;

					std::string relativePath = _path + fileName;

					// check whether it's already loaded
					DirIterMap::iterator iTemp = _dirIterMap.find(relativePath);
					if (iTemp != _dirIterMap.end())
						return;

					// Add the fields to the treeview
					GtkTreeIter iter;
					gtk_tree_store_append(_store, &iter, parIter);
					gtk_tree_store_set(_store, &iter, NAME_COLUMN, fileName.c_str(), DIRNAME_COLUMN,
							"", FULLNAME_COLUMN, relativePath.c_str(), SKIN_COLUMN, "", IMAGE_COLUMN,
							gtkutil::getLocalPixbuf(imgPath), -1);
					GtkTreeIter* dynIter = gtk_tree_iter_copy(&iter); // get a heap-allocated iter

					// Load the model
					ModelLoader* loader = ModelLoader_forType(os::getExtension(fileName));
					if (loader != NULL) {
						model::IModelPtr model = loader->loadModelFromPath(relativePath);
						if (model.get() != NULL) {
							// Update the text in the dialog
							_dialog.setText(relativePath);

							// Get the list of skins for this model. The number of skins is appended
							// to the model node name in brackets.
							ModelSkinList skins = model->getSkinsForModel();

							// Determine if this model has any associated skins, and add them as
							// children. We also set the fullpath column to the model name for each skin.
							int index = 0;
							for (ModelSkinList::iterator i = skins.begin(); i != skins.end(); ++i) {
								GtkTreeIter skIter;
								gtk_tree_store_append(_store, &skIter, &iter);
								gtk_tree_store_set(_store, &skIter, NAME_COLUMN, i->c_str(), DIRNAME_COLUMN, "", FULLNAME_COLUMN,
										relativePath.c_str(), SKIN_COLUMN, i->c_str(), SKIN_INDEX,
										index, IMAGE_COLUMN, gtkutil::getLocalPixbuf(ui::icons::ICON_SKIN), -1);
								index++;
							}
						} else {
							globalWarningStream() << "Could not load model " << relativePath << "\n";
						}
					}

					// Now add a map entry that maps our directory name to the row we just
					// added
					_dirIterMap[relativePath] = dynIter;
				}

				// Functor operator
				void operator() (const std::string& file)
				{
					GtkTreeIter* parIter = NULL;
					DirIterMap::iterator iTemp = _dirIterMap.find(_path);
					if (iTemp != _dirIterMap.end())
						parIter = iTemp->second;

					if (_scanForDirectory)
						addDirectory(file, parIter);
					else
						addModel(file, parIter);
				}
		};
	}

	// Destructor. The GtkTreeIters are dynamically allocated so we must free them
	ModelSelector::~ModelSelector ()
	{
		for (DirIterMap::iterator i = _dirIterMap.begin(); i != _dirIterMap.end(); ++i) {
			gtk_tree_iter_free(i->second);
		}
	}

	void ModelSelector::loadDirectory(const std::string& path) {
		if (path.empty())
			return;

		// Modal dialog window to display progress
		gtkutil::ModalProgressDialog dialog(GlobalRadiant().getMainWindow(), string::format(_("Loading models %s"), path.c_str()));

		// Populate the treestore using the VFS callback functor
		ModelFileFunctor functor(_treeStore, dialog, DirectoryCleaned(path), _dirIterMap);
		functor.setDirectory(true);
		GlobalFileSystem().forEachDirectory(path, makeCallback1(functor), 1);
		functor.setDirectory(false);
		GlobalFileSystem().forEachFile(path, "*", makeCallback1(functor), 1);
	}

	// Helper function to create the TreeView
	GtkWidget* ModelSelector::createTreeView ()
	{
		loadDirectory(MODELS_FOLDER);

		GtkTreeModel *model = gtk_tree_model_filter_new(GTK_TREE_MODEL(_treeStore), NULL);
		GtkTreeModel *modelSorted = gtk_tree_model_sort_new_with_model(model);

		GtkWidget* treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(modelSorted));
		// Single column, containing model pathname
		GtkTreeViewColumn* col = gtk_tree_view_column_new();
		gtk_tree_view_column_set_title(col, _("Value"));
		gtk_tree_view_column_set_spacing(col, 3);

		GtkCellRenderer* pixRenderer = gtk_cell_renderer_pixbuf_new();
		gtk_tree_view_column_pack_start(col, pixRenderer, FALSE);
		gtk_tree_view_column_set_attributes(col, pixRenderer, "pixbuf", IMAGE_COLUMN, NULL);

		GtkCellRenderer* rend = gtk_cell_renderer_text_new();
		gtk_tree_view_column_pack_start(col, rend, FALSE);
		gtk_tree_view_column_set_attributes(col, rend, "text", NAME_COLUMN, NULL);

		gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), col);
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeView), FALSE);

		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(modelSorted), NAME_COLUMN, GTK_SORT_ASCENDING);

		// Pack treeview into a scrolled window and return

		_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
		g_signal_connect(G_OBJECT(_selection), "changed", G_CALLBACK(callbackSelChanged), this);

		// Pack treeview into a scrolled window and frame, and return

		return gtkutil::ScrolledFrame(treeView);
	}

	// Create the buttons panel at bottom of dialog
	GtkWidget* ModelSelector::createButtons ()
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

	// Create the preview widget and info panel
	GtkWidget* ModelSelector::createPreviewAndInfoPanel ()
	{
		// This is a HBox with the preview GL widget on the left, and an info TreeView on the
		// right
		GtkWidget* hbx = gtk_hbox_new(FALSE, 3);

		// Pack in the GL widget, which is already created
		gtk_box_pack_start(GTK_BOX(hbx), _modelPreview, FALSE, FALSE, 0);

		// Info table. Has key and value columns.
		GtkWidget* infTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_infoStore));
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(infTreeView), FALSE);

		GtkCellRenderer* rend;
		GtkTreeViewColumn* col;

		rend = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(_("Attribute"), rend, "text", 0, NULL);
		g_object_set(G_OBJECT(rend), "weight", 700, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(infTreeView), col);

		rend = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(_("Value"), rend, "text", 1, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(infTreeView), col);

		// Pack into scroll window and frame

		GtkWidget* scroll = gtkutil::ScrolledFrame(infTreeView);

		gtk_box_pack_start(GTK_BOX(hbx), scroll, TRUE, TRUE, 0);

		// Return the HBox
		return hbx;
	}

	// Get the value from the selected column
	std::string ModelSelector::getSelectedString (gint colNum)
	{
		return gtkutil::TreeModel::getSelectedString(_selection, colNum);
	}

	// Get the value from the selected column
	int ModelSelector::getSelectedInteger (gint colNum)
	{
		// Get the selection
		GtkTreeIter iter;
		GtkTreeModel* model;

		if (gtk_tree_selection_get_selected(_selection, &model, &iter)) {
			// Get the value
			GValue val;
			val.g_type = 0;
			gtk_tree_model_get_value(model, &iter, colNum, &val);
			// Get the integer
			return g_value_get_int(&val);
		} else {
			// Nothing selected, return empty string
			return -1;
		}
	}

	// Update the info table and model preview based on the current selection
	void ModelSelector::updateSelected ()
	{
		// Prepare to populate the info table
		gtk_list_store_clear(_infoStore);
		GtkTreeIter iter;

		// Get the model name, if this is blank we are looking at a directory,
		// so leave the table empty
		std::string mName = getSelectedString(FULLNAME_COLUMN);
		if (mName.empty())
			return;

		// Get the skin if set
		std::string skinName = getSelectedString(SKIN_COLUMN);

		// Pass the model and skin to the preview widget
		_modelPreview.setModel(mName);
		_modelPreview.setSkin(skinName);

		// Update the text in the info table
		gtk_list_store_append(_infoStore, &iter);
		gtk_list_store_set(_infoStore, &iter, 0, _("Model name"), 1, mName.c_str(), -1);

		gtk_list_store_append(_infoStore, &iter);
		gtk_list_store_set(_infoStore, &iter, 0, _("Skin name"), 1, skinName.c_str(), -1);

		gtk_list_store_append(_infoStore, &iter);
		gtk_list_store_set(_infoStore, &iter, 0, _("Material surfaces"), 1,
				_modelPreview.getModel()->getSurfaceCountStr().c_str(), -1);
		gtk_list_store_append(_infoStore, &iter);
		gtk_list_store_set(_infoStore, &iter, 0, _("Total vertices"), 1,
				_modelPreview.getModel()->getVertexCountStr().c_str(), -1);
		gtk_list_store_append(_infoStore, &iter);
		gtk_list_store_set(_infoStore, &iter, 0, _("Total polys"), 1, _modelPreview.getModel()->getPolyCountStr().c_str(),
				-1);
	}

	/* GTK CALLBACKS */

	void ModelSelector::callbackHide (GtkWidget* widget, GdkEvent* ev, ModelSelector* self)
	{
		self->_lastModel = "";
		self->_lastSkin = -1;
		gtk_main_quit(); // exit recursive main loop
		gtk_widget_hide(self->_widget);
	}

	void ModelSelector::callbackSelChanged (GtkWidget* widget, ModelSelector* self)
	{
		std::string mName = self->getSelectedString(DIRNAME_COLUMN);
		self->loadDirectory(mName);

		self->updateSelected();
	}

	void ModelSelector::callbackOK (GtkWidget* widget, ModelSelector* self)
	{
		// Remember the selected model then exit from the recursive main loop
		self->_lastModel = self->getSelectedString(FULLNAME_COLUMN);
		// TODO: This must not be the name, but the skin index
		self->_lastSkin = self->getSelectedInteger(SKIN_INDEX);
		gtk_main_quit();
		gtk_widget_hide(self->_widget);
	}

	void ModelSelector::callbackCancel (GtkWidget* widget, ModelSelector* self)
	{
		self->_lastModel = "";
		self->_lastSkin = -1;
		gtk_main_quit();
		gtk_widget_hide(self->_widget);
	}
} // namespace ui
