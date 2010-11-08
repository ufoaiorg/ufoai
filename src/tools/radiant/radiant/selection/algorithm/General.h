#ifndef SELECTION_ALGORITHM_GENERAL_H_
#define SELECTION_ALGORITHM_GENERAL_H_

#include <list>
#include <string>
#include "iscenegraph.h"
#include "ientity.h"
#include "math/Vector3.h"
#include "math/aabb.h"

namespace selection {
	namespace algorithm {

	/**
	 * greebo: Each selected item will be deselected and vice versa.
	 */
	void invertSelection();
	/**
	 * greebo: Deletes all selected nodes including empty entities.
	 *
	 * Note: this command does not create an UndoableCommand, it only
	 *       contains the deletion algorithm.
	 */
	void deleteSelection();

	/**
	 * greebo: As the name says, these are the various selection routines.
	 */
	void selectInside();
	void selectTouching();

	} // namespace algorithm
} // namespace selection

#endif /* SELECTION_ALGORITHM_GENERAL_H_ */
