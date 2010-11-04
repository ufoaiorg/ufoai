#include "Primitives.h"

#include "../../brush/FaceInstance.h"
#include "string/string.h"

// greebo: Nasty global that contains all the selected face instances
extern FaceInstanceSet g_SelectedFaceInstances;

namespace selection {
	namespace algorithm {

int selectedFaceCount() {
	return static_cast<int>(g_SelectedFaceInstances.size());
}

Face& getLastSelectedFace() {
	if (selectedFaceCount() == 1) {
		return g_SelectedFaceInstances.last().getFace();
	}
	else {
		throw selection::InvalidSelectionException(string::toString(selectedFaceCount()));
	}
}

	} // namespace algorithm
} // namespace selection
