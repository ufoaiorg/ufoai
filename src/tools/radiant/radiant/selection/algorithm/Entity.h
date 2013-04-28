#pragma once

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
