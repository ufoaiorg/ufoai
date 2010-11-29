#include "Entity.h"

#include "radiant_i18n.h"
#include "selectionlib.h"
#include "iregistry.h"
#include "itextstream.h"
#include "entitylib.h"
#include "gtkutil/dialog.h"

#include "../../entity/entity.h"
#include "../../map/map.h"

namespace selection {
namespace algorithm {

class EntityCopyingVisitor: public Entity::Visitor
{
		Entity& m_entity;
	public:
		EntityCopyingVisitor (Entity& entity) :
			m_entity(entity)
		{
		}

		void visit (const std::string& key, const std::string& value)
		{
			if (key != "classname" && m_entity.getEntityClass().getAttribute(key) != NULL)
				m_entity.setKeyValue(key, value);
		}
};

void connectSelectedEntities ()
{
	if (GlobalSelectionSystem().countSelected() == 2) {
		GlobalEntityCreator().connectEntities(GlobalSelectionSystem().penultimateSelected().path(), // source
				GlobalSelectionSystem().ultimateSelected().path() // target
		);
	} else {
		gtkutil::errorDialog(_("Exactly two entities must be selected for this operation."));
	}
}

class EntityFindSelected: public scene::Graph::Walker
{
	public:
		mutable const scene::Path *groupPath;
		mutable scene::Instance *groupInstance;

		EntityFindSelected () :
			groupPath(0), groupInstance(0)
		{
		}

		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			return true;
		}

		void post (const scene::Path& path, scene::Instance& instance) const
		{
			Entity* entity = Node_getEntity(path.top());
			if (entity != 0 && Instance_getSelectable(instance)->isSelected() && node_is_group(path.top())
					&& !groupPath) {
				groupPath = &path;
				groupInstance = &instance;
			}
		}
};

class EntityGroupSelected: public scene::Graph::Walker
{
		NodeSmartReference group;
		NodeSmartReference worldspawn;
	public:
		EntityGroupSelected (const scene::Path &p) :
			group(p.top().get()), worldspawn(GlobalMap().findOrInsertWorldspawn())
		{
		}

		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			return true;
		}

		void post (const scene::Path& path, scene::Instance& instance) const
		{
			if (Instance_getSelectable(instance)->isSelected()) {
				Entity* entity = Node_getEntity(path.top());
				if (entity == 0 && Node_isPrimitive(path.top())) {
					NodeSmartReference child(path.top().get());
					NodeSmartReference parent(path.parent().get());

					if (path.size() >= 3 && parent != worldspawn) {
						NodeSmartReference parentparent(path[path.size() - 3].get());
						Node_getTraversable(parent)->erase(child);
						Node_getTraversable(group)->insert(child);

						if (Node_getTraversable(parent)->empty()) {
							Node_getTraversable(parentparent)->erase(parent);
						}
					} else {
						Node_getTraversable(parent)->erase(child);
						Node_getTraversable(group)->insert(child);
					}
				}
			}
		}
};

/**
 * @brief Moves all selected brushes into the selected entity.
 * Usage:
 *
 * - Select brush from entity
 * - Hit Ctrl-Alt-E
 * - Select some other brush
 * - Regroup
 *
 * The other brush will get added to the entity.
 *
 * - Select brush from entity
 * - Regroup
 * The brush will get removed from the entity, and moved to worldspawn.
 */
void groupSelected ()
{
	if (GlobalSelectionSystem().countSelected() < 1)
		return;

	UndoableCommand undo("groupSelectedEntities");

	scene::Path world_path(makeReference(GlobalSceneGraph().root()));
	world_path.push(makeReference(GlobalMap().findOrInsertWorldspawn()));

	EntityFindSelected fse;
	GlobalSceneGraph().traverse(fse);
	if (fse.groupPath) {
		GlobalSceneGraph().traverse(EntityGroupSelected(*fse.groupPath));
	} else {
		GlobalSceneGraph().traverse(EntityGroupSelected(world_path));
	}
}

void ungroupSelected ()
{
	if (GlobalSelectionSystem().countSelected() < 1)
		return;

	UndoableCommand undo("ungroupSelectedEntities");

	scene::Path world_path(makeReference(GlobalSceneGraph().root()));
	world_path.push(makeReference(GlobalMap().findOrInsertWorldspawn()));

	scene::Instance &instance = GlobalSelectionSystem().ultimateSelected();
	scene::Path path = instance.path();

	if (!Node_isEntity(path.top()))
		path.pop();

	if (Node_getEntity(path.top()) != 0 && node_is_group(path.top())) {
		if (world_path.top().get_pointer() != path.top().get_pointer()) {
			parentBrushes(path.top(), world_path.top());
			Path_deleteTop(path);
		}
	}
}

} // namespace algorithm
} // namespace selection
