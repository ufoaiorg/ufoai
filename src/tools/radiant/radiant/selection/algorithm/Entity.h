#ifndef SELECTION_ALGORITHM_ENTITY_H_
#define SELECTION_ALGORITHM_ENTITY_H_

#include <string>

namespace selection {
	namespace algorithm {

	/**
	 * greebo: Sets up the target spawnarg of the selected entities such that
	 * the first selected entity is targetting the second.
	 */
	void connectSelectedEntities();

	void groupSelected ();

	void ungroupSelected ();

	} // namespace algorithm
} // namespace selection

#endif /* SELECTION_ALGORITHM_ENTITY_H_ */
