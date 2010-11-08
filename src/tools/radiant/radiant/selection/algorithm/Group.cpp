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
#include "../../entity.h"

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

} // namespace algorithm

} // namespace selection
