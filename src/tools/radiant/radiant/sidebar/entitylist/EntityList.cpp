#include "../sidebar.h"
#include "EntityList.h"

#include "radiant_i18n.h"
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

EntityList::EntityList () :
		_callbackActive(false)
{
	// Be sure to pass FALSE to the TransientWindow to prevent it from self-destruction
	_widget = gtk_vbox_new(FALSE, 0);

	// Create all the widgets and pack them into the window
	populateWindow();
}

EntityList::~EntityList ()
{
	_treeModel.clear();
}

void EntityList::populateWindow ()
{
	_treeView = GTK_TREE_VIEW(gtk_tree_view_new());
	gtk_tree_view_set_headers_visible(_treeView, FALSE);

	gtk_tree_view_set_model(_treeView, _treeModel);

	GtkTreeViewColumn* column = gtkutil::TextColumn("Name", GraphTreeModel::COL_NAME);
	gtk_tree_view_column_pack_start(column, gtk_cell_renderer_text_new(), TRUE);

	_selection = gtk_tree_view_get_selection(_treeView);
	gtk_tree_selection_set_mode(_selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function(_selection, onSelection, this, 0);

	g_signal_connect(G_OBJECT(_treeView), "row-expanded", G_CALLBACK(onRowExpand), this);

	gtk_tree_view_append_column(_treeView, column);

	gtk_container_add(GTK_CONTAINER(_widget), gtkutil::ScrolledFrame(GTK_WIDGET(_treeView)));
}

void EntityList::update ()
{
	// Disable callbacks and traverse the treemodel
	_callbackActive = true;
	// Traverse the entire tree, updating the selection
	_treeModel.updateSelectionStatus(_selection);
	_callbackActive = false;
}

// Gets notified upon selection change
void EntityList::selectionChanged (scene::Instance& instance, bool isComponent)
{
	if (_callbackActive || isComponent)
		return; // avoid loops

	_callbackActive = true;
	_treeModel.updateSelectionStatus(_selection, instance);
	_callbackActive = false;
}

GtkWidget* EntityList::getWidget () const
{
	return _widget;
}

const std::string EntityList::getTitle () const
{
	return _("EntityList");
}

void EntityList::switchPage (int pageIndex)
{
	if (pageIndex == _pageIndex) {
		// Register self to the SelSystem to get notified upon selection changes.
		GlobalSelectionSystem().addObserver(this);
		_callbackActive = true;
		// Repopulate the model before showing the dialog\r
		_treeModel.refresh();
		_callbackActive = false;
		update();
	} else
		GlobalSelectionSystem().removeObserver(this);
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
	EntityList* self = reinterpret_cast<EntityList*>(data);

	if (self->_callbackActive)
		return TRUE; // avoid loops

	GtkTreeIter iter;
	gtk_tree_model_get_iter(model, &iter, path);

	// Load the instance pointer from the columns
	scene::Instance& instance = *reinterpret_cast<scene::Instance*>(gtkutil::TreeModel::getPointer(model, &iter,
			GraphTreeModel::COL_INSTANCE_POINTER));

	Selectable *selectable = Instance_getSelectable(instance);

	if (selectable != NULL) {
		// We've found a selectable instance

		// Disable update to avoid loopbacks
		self->_callbackActive = true;

		// Select the instance
		selectable->setSelected(path_currently_selected == FALSE);

		const AABB& aabb = instance.worldAABB();
		Vector3 origin(aabb.origin);
		// Move the camera a bit off the AABB origin
		origin += Vector3(-50, 0, 50);
		// Rotate the camera a bit towards the "ground"
		GlobalMap().FocusViews(origin, -30);

		// Now reactivate the callbacks
		self->_callbackActive = false;

		return TRUE; // don't propagate
	}

	return FALSE;
}

} // namespace ui
