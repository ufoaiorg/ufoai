#include "ModelSelector.h"

#include "radiant_i18n.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/image.h"
#include "gtkutil/ScrolledFrame.h"
#include "ifilesystem.h"
#include "iradiant.h"
#include "../../referencecache.h"
#include "os/path.h"
#include "../Icons.h"
#include "../../mainframe.h" // ScopeDisableScreenUpdates
#include <cmath>
#include <iostream>
#include <vector>
#include <sstream>
#include <map>

namespace ui
{
	// CONSTANTS

	namespace
	{
		const char* MODELSELECTOR_TITLE = _("Choose model");
		const char* MODELS_FOLDER = "models/";
		const char* MD2_EXTENSION = "md2";
		const char* MD3_EXTENSION = "md3";
		const char* OBJ_EXTENSION = "obj";

		// Treestore enum
		enum
		{
			NAME_COLUMN, // e.g. "chair1.md2"
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
				G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, GDK_TYPE_PIXBUF)), _infoStore(gtk_list_store_new(2,
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
		gint glSize = gint(h * 0.4);
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
				typedef const char* first_argument_type;

				// Tree store to populate

				GtkTreeStore* _store;

				// Map between model directory names (e.g. "models/objects") and
				// a GtkTreeIter pointing to the equivalent row in the TreeModel. Subsequent
				// modelpaths with this directory will be added as children of this iter.

				typedef std::map<std::string, GtkTreeIter*> DirIterMap;
				DirIterMap _dirIterMap;

				// Constructor

				ModelFileFunctor (GtkTreeStore* store) :
					_store(store)
				{
				}

				// Destructor. The GtkTreeIters are dynamically allocated so we must free them

				~ModelFileFunctor ()
				{
					for (DirIterMap::iterator i = _dirIterMap.begin(); i != _dirIterMap.end(); ++i) {
						gtk_tree_iter_free(i->second);
					}
				}

				// Recursive function to add a given model path ("models/something/model.ext")
				// to its correct place in the tree. This is done by maintaining a cache of
				// directory nodes ("models/something", "models/") against
				// GtkIters that point to the corresponding row in the tree model. On each call,
				// the parent node is recursively calculated, and the node provided as an argument
				// added as a child.

				GtkTreeIter* addRecursive (const std::string& dirPath)
				{
					// We first try to lookup the directory name in the map. Return it
					// if it exists, otherwise recursively obtain the parent of this directory name,
					// and add this directory as a child in the tree model. We also add this
					// directory to the map for future lookups.

					DirIterMap::iterator iTemp = _dirIterMap.find(dirPath);
					if (iTemp != _dirIterMap.end()) { // found in map
						return iTemp->second;
					} else {
						// Perform the search for final "/" which identifies the parent
						// of this directory, and call recursively. If there is no slash, we
						// are looking at a toplevel directory in which case the parent is
						// NULL.
						size_t slashPos = dirPath.rfind("/");
						GtkTreeIter* parIter = NULL;

						if (slashPos != std::string::npos) {
							parIter = addRecursive(dirPath.substr(0, slashPos));
						}

						/* Add this directory to the treemodel. For the displayed tree, we
						 ** want the last component of the directory name, not the entire path
						 ** at each node.
						 */
						std::stringstream nodeName;
						nodeName << dirPath.substr(slashPos + 1);

						// Decide which image to use, based on the file extension (or the folder
						// image if there is no extension). Also, set a flag indicating that we
						// have an actual model rather than a directory, so that the fullname
						// tree column can be populated

						std::string imgPath = ui::icons::ICON_FOLDER;
						bool isModel = false;

						if (os::getExtension(dirPath) == MD3_EXTENSION) {
							imgPath = ui::icons::ICON_MD3;
							isModel = true;
						} else if (os::getExtension(dirPath) == MD2_EXTENSION) {
							imgPath = ui::icons::ICON_MD2;
							isModel = true;
						} else if (os::getExtension(dirPath) == OBJ_EXTENSION) {
							imgPath = ui::icons::ICON_OBJ;
							isModel = true;
						}

						// Add the fields to the treeview

						GtkTreeIter iter;
						gtk_tree_store_append(_store, &iter, parIter);
						gtk_tree_store_set(_store, &iter, NAME_COLUMN, nodeName.str().c_str(), FULLNAME_COLUMN,
								(isModel ? (MODELS_FOLDER + dirPath).c_str() : ""), SKIN_COLUMN, "", IMAGE_COLUMN,
								gtkutil::getLocalPixbuf(imgPath), -1);
						GtkTreeIter* dynIter = gtk_tree_iter_copy(&iter); // get a heap-allocated iter

						if (isModel) {
							// Load the model
							ModelLoader* loader = ModelLoader_forType(os::getExtension(dirPath));
							if (loader != NULL) {
								model::IModelPtr model = loader->loadModelFromPath(MODELS_FOLDER + dirPath);

								// Get the list of skins for this model. The number of skins is appended
								// to the model node name in brackets.
								ModelSkinList skins = model->getSkinsForModel();
								int numSk = skins.size();
								if (numSk > 0) {
									nodeName << " [" << numSk << (numSk == 1 ? " skin]" : " skins]");
								}

								// Determine if this model has any associated skins, and add them as
								// children. We also set the fullpath column to the model name for each skin.
								int index = 0;
								for (ModelSkinList::iterator i = skins.begin(); i != skins.end(); ++i) {
									GtkTreeIter skIter;
									gtk_tree_store_append(_store, &skIter, &iter);
									gtk_tree_store_set(_store, &skIter, NAME_COLUMN, i->c_str(), FULLNAME_COLUMN,
											(MODELS_FOLDER + dirPath).c_str(), SKIN_COLUMN, i->c_str(), SKIN_INDEX,
											index, IMAGE_COLUMN, gtkutil::getLocalPixbuf(ui::icons::ICON_SKIN), -1);
									index++;
								}
							}
						}

						// Now add a map entry that maps our directory name to the row we just
						// added
						_dirIterMap[dirPath] = dynIter;

						// Return our new dynamic iter.
						return dynIter;
					}
				}

				// Functor operator
				void operator() (const char* file)
				{
					std::string rawPath(file);

					// Test the extension for supported model formats.
					std::string ext = os::getExtension(file);
					if (ext == MD2_EXTENSION || ext == MD3_EXTENSION || ext == OBJ_EXTENSION) {
						addRecursive(rawPath);
					}
				}
		};
	}

	// Helper function to create the TreeView
	GtkWidget* ModelSelector::createTreeView ()
	{
		ScopeDisableScreenUpdates load(_("Loading..."), _("Please wait"));

		// Populate the treestore using the VFS callback functor
		ModelFileFunctor functor(_treeStore);
		GlobalFileSystem().forEachFile(MODELS_FOLDER, "*", makeCallback1(functor), 0);

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

		GtkWidget* scrollWin = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollWin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_container_add(GTK_CONTAINER(scrollWin), treeView);

		GtkWidget* fr = gtk_frame_new(NULL);
		gtk_container_add(GTK_CONTAINER(fr), scrollWin);

		return fr;
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

		GtkWidget* frame = gtk_frame_new(NULL);
		gtk_container_add(GTK_CONTAINER(frame), scroll);

		gtk_box_pack_start(GTK_BOX(hbx), frame, TRUE, TRUE, 0);

		// Return the HBox
		return hbx;
	}

	// Get the value from the selected column
	std::string ModelSelector::getSelectedString (gint colNum)
	{
		// Get the selection
		GtkTreeIter iter;
		GtkTreeModel* model;

		if (gtk_tree_selection_get_selected(_selection, &model, &iter)) {
			// Get the value
			GValue val;
			memset(&val, 0, sizeof(val));
			gtk_tree_model_get_value(model, &iter, colNum, &val);
			// Get the string
			return g_value_get_string(&val);
		} else {
			// Nothing selected, return empty string
			return "";
		}
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
			memset(&val, 0, sizeof(val));
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
				_modelPreview.getModel()->getSurfaceCount().c_str(), -1);
		gtk_list_store_append(_infoStore, &iter);
		gtk_list_store_set(_infoStore, &iter, 0, _("Total vertices"), 1,
				_modelPreview.getModel()->getVertexCount().c_str(), -1);
		gtk_list_store_append(_infoStore, &iter);
		gtk_list_store_set(_infoStore, &iter, 0, _("Total polys"), 1, _modelPreview.getModel()->getPolyCount().c_str(),
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
