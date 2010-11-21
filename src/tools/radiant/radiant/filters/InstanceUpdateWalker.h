#ifndef INSTANCEUPDATEWALKER_H_
#define INSTANCEUPDATEWALKER_H_

#include "iscenegraph.h"
#include "ientity.h"
#include "ieclass.h"

#include "scenelib.h"
#include "eclasslib.h"

#include "../brush/BrushNode.h"

namespace filters {

// Walker: de-selects a complete subgraph
class Deselector: public scene::Traversable::Walker
{
	public:
		bool pre (scene::Node& node) const
		{
			scene::Instance* instance = GlobalSceneGraph().find(node);
			Instance_getSelectable(*instance)->setSelected(false);
			return true;
		}
};

// Walker: Shows or hides a complete subgraph
class NodeVisibilityUpdater: public scene::Traversable::Walker
{
	private:
		bool _filtered;

	public:
		NodeVisibilityUpdater (bool setFiltered) :
			_filtered(setFiltered)
		{
		}

		bool pre (scene::Node& node) const
		{
			node.setFiltered(_filtered);
			return true;
		}
};

/**
 * Scenegraph walker to update filtered status of Instances based on the
 * status of their parent entity class.
 */
class InstanceUpdateWalker: public scene::Graph::Walker
{
		// Helper visitors to update subgraphs
		NodeVisibilityUpdater _hideWalker;
		NodeVisibilityUpdater _showWalker;

		// Cached boolean to avoid GlobalFilterSystem() queries for each node
		bool _brushesAreVisible;

	public:

		InstanceUpdateWalker () :
			_hideWalker(true), _showWalker(false),
					_brushesAreVisible(GlobalFilterSystem().isVisible("object", "brush"))
		{
		}

		// Pre-descent walker function
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			scene::Node& node = path.top();
			// Retrieve the parent entity and check its entity class.
			Entity* entity = Node_getEntity(node);
			if (entity) {
				const EntityClass& eclass = entity->getEntityClass();
				const std::string& name = eclass.name();
				bool entityClassVisible = GlobalFilterSystem().isVisible("entityclass", name);
				Node_traverseSubgraph(node, entityClassVisible ? _showWalker : _hideWalker);
			}

			// greebo: Update visibility of BrushInstances
			if (Node_isBrush(node)) {
				Brush* brush = Node_getBrush(node);
				bool isVisible = _brushesAreVisible && brush->hasVisibleMaterial();
				Node_traverseSubgraph(node, isVisible ? _showWalker : _hideWalker);
			}

			if (!node.visible()) {
				// de-select this node and all children
				Deselector deselector;
				Node_traverseSubgraph(node, deselector);
			}

			// Continue the traversal
			return true;
		}
};

}

#endif /*INSTANCEUPDATEWALKER_H_*/
