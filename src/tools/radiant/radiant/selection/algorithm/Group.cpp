#include "Group.h"

#include "radiant_i18n.h"
#include <set>
#include "iradiant.h"
#include "itextstream.h"
#include "selectionlib.h"
#include "scenelib.h"
#include "entitylib.h"
#include "../../map/map.h"
#include "gtkutil/dialog.h"
#include "../../entity/entity.h"

namespace selection {

namespace algorithm {

class ExpandSelectionToEntitiesWalker: public scene::Graph::Walker
{
		mutable std::size_t m_depth;
	public:
		ExpandSelectionToEntitiesWalker (void) :
			m_depth(0)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			++m_depth;
			if (m_depth == 2) { /* entity depth */
				/* traverse and select children if any one is selected */
				if (instance.childSelected())
					Instance_setSelected(instance, true);
				return Node_getEntity(path.top())->isContainer() && instance.childSelected();
			} else if (m_depth == 3) { /* primitive depth */
				Instance_setSelected(instance, true);
				return false;
			}
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			--m_depth;
		}
};

void expandSelectionToEntities ()
{
	GlobalSceneGraph().traverse(ExpandSelectionToEntitiesWalker());
}

class ParentSelectedBrushesToEntityWalker: public SelectionSystem::Visitor
{
	private:
		const scene::Path& m_parent;

		bool contains_entity (scene::Node& node) const
		{
			return Node_getTraversable(node) != 0 && !Node_isBrush(node) && !Node_isEntity(node);
		}

		bool contains_primitive (scene::Node& node) const
		{
			return Node_isEntity(node) && Node_getTraversable(node) != 0 && Node_getEntity(node)->isContainer();
		}

		ENodeType node_get_contains (scene::Node& node) const
		{
			if (contains_entity(node)) {
				return eNodeEntity;
			}
			if (contains_primitive(node)) {
				return eNodePrimitive;
			}
			return eNodeUnknown;
		}

		void Path_parent (const scene::Path& parent, const scene::Path& child) const
		{
			ENodeType contains = node_get_contains(parent.top());
			ENodeType type = node_get_nodetype(child.top());

			if (contains != eNodeUnknown && contains == type) {
				NodeSmartReference node(child.top().get());
				Path_deleteTop(child);
				Node_getTraversable(parent.top())->insert(node);
				SceneChangeNotify();
			} else {
				globalErrorStream() << "failed - " << nodetype_get_name(type) << " cannot be parented to "
						<< nodetype_get_name(contains) << " container.\n";
			}
		}

	public:
		ParentSelectedBrushesToEntityWalker (const scene::Path& parent) :
			m_parent(parent)
		{
		}
		void visit (scene::Instance& instance) const
		{
			if (&m_parent != &instance.path()) {
				Path_parent(m_parent, instance.path());
			}
		}
};

bool curSelectionIsSuitableForReparent()
{
	// Retrieve the selection information structure
	const SelectionInfo& info = GlobalSelectionSystem().getSelectionInfo();

	if (info.totalCount <= 1 || info.entityCount != 1)
	{
		return false;
	}

	Entity* entity = Node_getEntity(GlobalSelectionSystem().ultimateSelected().path().top().get());

	if (entity == NULL)
	{
		return false;
	}

	return true;
}

// re-parents the selected brushes/patches
void parentSelection()
{
	// Retrieve the selection information structure
	if (!curSelectionIsSuitableForReparent())
	{
		gtkutil::errorDialog(_("Cannot reparent primitives to entity. "
						 "Please select at least one brush and exactly one entity."
						 "(The entity has to be selected last.)"));
		return;
	}

	UndoableCommand undo("parentSelectedPrimitives");

	// Take the last selected item (this is an entity)
	ParentSelectedBrushesToEntityWalker visitor(GlobalSelectionSystem().ultimateSelected().path());
	GlobalSelectionSystem().foreachSelected(visitor);
}

} // namespace algorithm

} // namespace selection
