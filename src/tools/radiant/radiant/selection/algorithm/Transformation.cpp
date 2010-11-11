#include "Transformation.h"

#include <string>
#include "math/quaternion.h"
#include "iundo.h"
#include "scenelib.h"
#include "iselection.h"
#include "iscenegraph.h"
#include "gtkutil/dialog.h"
#include "../../mainframe.h"
#include "../../map/map.h"
#include "radiant_i18n.h"

namespace selection {
	namespace algorithm {

// greebo: see header for documentation
void rotateSelected(const Vector3& eulerXYZ) {
	std::string command("rotateSelectedEulerXYZ: ");
	command += eulerXYZ;
	UndoableCommand undo(command);

	GlobalSelectionSystem().rotateSelected(quaternion_for_euler_xyz_degrees(eulerXYZ));
}

// greebo: see header for documentation
void scaleSelected(const Vector3& scaleXYZ) {

	if (fabs(scaleXYZ[0]) > 0.0001f &&
		fabs(scaleXYZ[1]) > 0.0001f &&
		fabs(scaleXYZ[2]) > 0.0001f)
	{
		std::string command("scaleSelected: ");
		command += scaleXYZ;
		UndoableCommand undo(command);

		GlobalSelectionSystem().scaleSelected(scaleXYZ);
	}
	else {
		gtkutil::errorDialog(_("Cannot scale by zero value."));
	}
}

class CloneSelected: public scene::Graph::Walker
{
	public:
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.size() == 1)
				return true;

			if (!path.top().get().isRoot()) {
				Selectable* selectable = Instance_getSelectable(instance);
				if (selectable != 0 && selectable->isSelected()) {
					return false;
				}
			}

			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.size() == 1)
				return;

			if (!path.top().get().isRoot()) {
				Selectable* selectable = Instance_getSelectable(instance);
				if (selectable != 0 && selectable->isSelected()) {
					NodeSmartReference clone(Node_Clone(path.top()));
					Map_gatherNamespaced(clone);
					Node_getTraversable(path.parent().get())->insert(clone);
				}
			}
		}
};

void cloneSelected() {
	// Check for the correct editing mode (don't clone components)
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent) {
		return;
	}

	UndoableCommand undo("cloneSelected");

	GlobalSceneGraph().traverse(CloneSelected());

	Map_mergeClonedNames();
}

	} // namespace algorithm
} // namespace selection
