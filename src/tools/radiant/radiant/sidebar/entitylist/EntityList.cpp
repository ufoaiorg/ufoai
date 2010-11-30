#include "EntityList.h"

#include "ieventmanager.h"
#include "iregistry.h"
#include "iradiant.h"
#include "nameable.h"
#include <gtk/gtk.h>
#include "gtkutil/TextColumn.h"
#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/TreeModel.h"
#include "entitylib.h"
#include "../../ui/mainframe/mainframe.h"
#include "../../map/map.h"
#include "scenelib.h"
#include "../../scenegraph/treemodel.h"
#include "../../camera/Camera.h"

namespace ui {

namespace {
enum
{
	NODE_COL, INSTANCE_COL, NUM_COLS
};
}

EntityList::EntityList () :
	_callbackActive(false)
{
	// Be sure to pass FALSE to the TransientWindow to prevent it from self-destruction
	_widget = gtk_vbox_new(FALSE, 0);

	// Create all the widgets and pack them into the window
	populateWindow();

	// Register self to the SelSystem to get notified upon selection changes.
	GlobalSelectionSystem().addObserver(this);
}

namespace {

inline Nameable* Node_getNameable(scene::Node& node) {
	return dynamic_cast<Nameable*>(&node);
}

std::string getNodeName(scene::Node& node) {
	Nameable* nameable = Node_getNameable(node);
	return (nameable != NULL) ? nameable->name() : "node";
}

void cellDataFunc (GtkTreeViewColumn* column, GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter,
		gpointer data)
{
	// Load the pointers from the columns
	scene::Node* node = reinterpret_cast<scene::Node*> (gtkutil::TreeModel::getPointer(model, iter, NODE_COL));

	scene::Instance* instance = reinterpret_cast<scene::Instance*> (gtkutil::TreeModel::getPointer(model, iter,
			INSTANCE_COL));

	if (node != NULL) {
		gtk_cell_renderer_set_fixed_size(renderer, -1, -1);
		std::string name = getNodeName(*node);
		g_object_set(G_OBJECT(renderer), "text", name.c_str(), "visible", TRUE, NULL);

		GtkWidget* treeView = reinterpret_cast<GtkWidget*> (data);

		GtkStyle* style = gtk_widget_get_style(treeView);
		if (instance->childSelected()) {
			g_object_set(G_OBJECT(renderer), "cell-background-gdk", &style->base[GTK_STATE_ACTIVE], NULL);
		} else {
			g_object_set(G_OBJECT(renderer), "cell-background-gdk", &style->base[GTK_STATE_NORMAL], NULL);
		}
	} else {
		gtk_cell_renderer_set_fixed_size(renderer, -1, 0);
		g_object_set(G_OBJECT(renderer), "text", "", "visible", FALSE, NULL);
	}
}
}

void EntityList::populateWindow ()
{
	_treeView = GTK_TREE_VIEW(gtk_tree_view_new());
	gtk_tree_view_set_headers_visible(_treeView, FALSE);

	_treeModel = GTK_TREE_MODEL(scene_graph_get_tree_model());
	gtk_tree_view_set_model(_treeView, _treeModel);

	GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
	GtkTreeViewColumn* column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func(column, renderer, cellDataFunc, _treeView, NULL);

	_selection = gtk_tree_view_get_selection(_treeView);
	gtk_tree_selection_set_mode(_selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function(_selection, onSelection, this, 0);

	g_signal_connect(G_OBJECT(_treeView), "row-expanded", G_CALLBACK(onRowExpand), this);

	gtk_tree_view_append_column(_treeView, column);

	gtk_container_add(GTK_CONTAINER(_widget), gtkutil::ScrolledFrame(GTK_WIDGET(_treeView)));
}

gboolean EntityList::modelUpdater (GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, gpointer data)
{
	GtkTreeView* treeView = reinterpret_cast<GtkTreeView*> (data);

	// Load the pointers from the columns
	scene::Instance* instance = reinterpret_cast<scene::Instance*> (gtkutil::TreeModel::getPointer(model, iter,
			INSTANCE_COL));

	Selectable* selectable = Instance_getSelectable(*instance);

	if (selectable != NULL) {
		GtkTreeSelection* selection = gtk_tree_view_get_selection(treeView);
		if (selectable->isSelected()) {
			gtk_tree_selection_select_path(selection, path);
		} else {
			gtk_tree_selection_unselect_path(selection, path);
		}
	}

	return FALSE;
}

void EntityList::update ()
{
	// Disable callbacks and traverse the treemodel
	_callbackActive = true;
	gtk_tree_model_foreach(_treeModel, modelUpdater, _treeView);
	_callbackActive = false;
}

// Gets notified upon selection change
void EntityList::selectionChanged ()
{
	if (_callbackActive)
		return; // avoid loops

	update();
}

EntityList& EntityList::Instance ()
{
	static EntityList _instance;
	return _instance;
}

GtkWidget* EntityList::getWidget () const
{
	return _widget;
}

void EntityList::onRowExpand (GtkTreeView* view, GtkTreeIter* iter, GtkTreePath* path, EntityList* self)
{
	if (self->_callbackActive)
		return; // avoid loops

	self->update();
}

gboolean EntityList::onSelection (GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path,
		gboolean path_currently_selected, gpointer data)
{
	// Get a pointer to the class instance
	EntityList* self = reinterpret_cast<EntityList*> (data);

	if (self->_callbackActive)
		return TRUE; // avoid loops

	GtkTreeIter iter;
	gtk_tree_model_get_iter(model, &iter, path);

	// Load the pointers from the columns
	scene::Node* node = reinterpret_cast<scene::Node*> (gtkutil::TreeModel::getPointer(model, &iter, NODE_COL));

	scene::Instance* instance = reinterpret_cast<scene::Instance*> (gtkutil::TreeModel::getPointer(model, &iter,
			INSTANCE_COL));

	Selectable* selectable = Instance_getSelectable(*instance);

	if (node == NULL) {
		if (path_currently_selected != FALSE) {
			// Disable callbacks
			self->_callbackActive = true;

			// Deselect all
			GlobalSelectionSystem().setSelectedAll(false);

			// Now reactivate the callbacks
			self->_callbackActive = false;
		}
	} else if (selectable != NULL) {
		// We've found a selectable instance

		// Disable callbacks
		self->_callbackActive = true;

		// Select the instance
		selectable->setSelected(path_currently_selected == FALSE);

		// greebo: Grab the origin keyvalue from the entity and focus the view on it
		Entity* entity = Node_getEntity(*node);
		if (entity != NULL) {
			Vector3 entityOrigin(entity->getKeyValue("origin"));

			// Move the camera a bit off the entity origin
			entityOrigin += Vector3(-50, 0, 50);

			// Rotate the camera a bit towards the "ground"
			Vector3 angles(0, 0, 0);
			//angles[CAMERA_PITCH] = -30;
			// TODO: mattn

			GlobalMap().FocusViews(entityOrigin, -30);
		}

		// Now reactivate the callbacks
		self->_callbackActive = false;

		return TRUE;
	}

	return FALSE;
}

} // namespace ui
