#ifndef ENTITYLIST_H_
#define ENTITYLIST_H_

#include "iselection.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkTreeView GtkTreeView;
typedef struct _GtkTreeSelection GtkTreeSelection;
typedef struct _GtkTreeIter GtkTreeIter;
typedef struct _GtkTreePath GtkTreePath;
typedef struct _GtkTreeModel GtkTreeModel;

namespace ui {

class EntityList :
	public SelectionSystem::Observer
{
	// The main dialog window
	GtkWidget* _widget;
	GtkTreeView* _treeView;
	GtkTreeSelection* _selection;

	// The treemodel (is hosted externally in scenegraph - legacy)
	GtkTreeModel* _treeModel;

	bool _callbackActive;

public:
	// Constructor, creates all the widgets
	EntityList();

	/** greebo: Contains the static instance. Use this
	 * 			to access the other members
	 */
	static EntityList& Instance();

	GtkWidget* getWidget() const;

private:
	/** greebo: Creates the widgets
	 */
	void populateWindow();

	/** greebo: Updates the treeview contents
	 */
	void update();

	/** greebo: SelectionSystem::Observer implementation.
	 * 			Gets notified as soon as the selection is changed.
	 */
	void selectionChanged();

	// The callback to catch the delete event (toggles the window)
	static void onRowExpand(GtkTreeView* view, GtkTreeIter* iter, GtkTreePath* path, EntityList* self);

	static gboolean onSelection(GtkTreeSelection *selection, GtkTreeModel *model,
								GtkTreePath *path, gboolean path_currently_selected, gpointer data);

	static gboolean modelUpdater(GtkTreeModel* model, GtkTreePath* path,
								 GtkTreeIter* iter, gpointer data);
};

} // namespace ui

#endif /*ENTITYLIST_H_*/
