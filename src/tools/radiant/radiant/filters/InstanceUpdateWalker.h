#ifndef INSTANCEUPDATEWALKER_H_
#define INSTANCEUPDATEWALKER_H_

#include "iscenegraph.h"
#include "ientity.h"
#include "ieclass.h"

#include "scenelib.h"
#include "eclasslib.h"

namespace filters {

/**
 * Scenegraph walker to update filtered status of Instances based on the
 * status of their parent entity class.
 */
class InstanceUpdateWalker: public scene::Graph::Walker
{
		// Cached boolean to avoid GlobalFilterSystem() queries for each node
		bool _brushesAreVisible;

	public:

		InstanceUpdateWalker () :
			_brushesAreVisible(GlobalFilterSystem().isVisible("object", "brush"))
		{
		}

		// Pre-descent walker function
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{

			// Retrieve the parent entity and check its entity class.
			Entity* entity = Node_getEntity(path.top());
			if (entity) {
				const EntityClass& eclass = entity->getEntityClass();
				const std::string& name = eclass.name();
				if (!GlobalFilterSystem().isVisible("entityclass", name)) {
					instance.setFiltered(true);
				} else {
					instance.setFiltered(false);
				}
			}

			// greebo: Update visibility of BrushInstances
			if (Node_isBrush(path.top())) {
				instance.setFiltered(!_brushesAreVisible);
			}

			// Continue the traversal
			return true;
		}

		// Post descent function
		void post (const scene::Path& path, scene::Instance& instance) const
		{

		}
};

}

#endif /*INSTANCEUPDATEWALKER_H_*/
