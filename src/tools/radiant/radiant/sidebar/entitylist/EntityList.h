#ifndef ENTITYLIST_H_
#define ENTITYLIST_H_

#include "iselection.h"
#include "iscenegraph.h"
#include "GraphTreeModel.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkTreeView GtkTreeView;
typedef struct _GtkTreeSelection GtkTreeSelection;
typedef struct _GtkTreeIter GtkTreeIter;
typedef struct _GtkTreePath GtkTreePath;
typedef struct _GtkTreeModel GtkTreeModel;

namespace ui {

/* FORWARD DECLS */
class SidebarComponent;

class EntityList: public SelectionSystem::Observer, public SidebarComponent {
	// The main dialog window
	GtkWidget* _widget;
	GtkTreeView* _treeView;
	GtkTreeSelection* _selection;

	// The GraphTreeModel instance
	ui::GraphTreeModel _treeModel;

	bool _callbackActive;

public:
	// Constructor, creates all the widgets
	EntityList ();
	~EntityList ();

	GtkWidget* getWidget () const;

	const std::string getTitle () const;

	void switchPage (int pageIndex);
private:
	/** greebo: Creates the widgets
	 */
	void populateWindow ();

	/** greebo: Updates the treeview contents
	 */
	void update ();

	/** greebo: SelectionSystem::Observer implementation.
	 * 			Gets notified as soon as the selection is changed.
	 */
	void selectionChanged (scene::Instance& instance, bool isComponent);

	// The callback to catch the delete event (toggles the window)
	static void onRowExpand (GtkTreeView* view, GtkTreeIter* iter, GtkTreePath* path, EntityList* self);

	static gboolean onSelection (GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path,
			gboolean path_currently_selected, gpointer data);
};

} // namespace ui

#endif /*ENTITYLIST_H_*/
