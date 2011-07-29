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
class Deselector: public scene::Graph::Walker
{
	public:
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			Instance_setSelected(instance, false);
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

	public:

		InstanceUpdateWalker () :
			_hideWalker(true), _showWalker(false)
		{
		}

		// Pre-descent walker function
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			scene::Node& node = path.top();
			// Retrieve the parent entity and check its entity class.
			Entity* entity = Node_getEntity(node);
			if (entity) {
				const bool entityClassVisible = GlobalFilterSystem().isEntityVisible(FilterRule::TYPE_ENTITYCLASS, *entity)
						&& GlobalFilterSystem().isEntityVisible(FilterRule::TYPE_ENTITYKEYVALUE, *entity);
				Node_traverseSubgraph(node, entityClassVisible ? _showWalker : _hideWalker);
				// If the entity is hidden, don't traverse the child nodes
				return entityClassVisible;
			}

			// greebo: Update visibility of BrushInstances
			if (Node_isBrush(node)) {
				Brush* brush = Node_getBrush(node);
				bool isVisible = brush->hasVisibleMaterial();
				Node_traverseSubgraph(node, isVisible ? _showWalker : _hideWalker);
			}

			if (!node.visible()) {
				// de-select this node and all children
				Deselector deselector;
				GlobalSceneGraph().traverse_subgraph(Deselector(), path);
			}

			// Continue the traversal
			return true;
		}
};

}

#endif /*INSTANCEUPDATEWALKER_H_*/
