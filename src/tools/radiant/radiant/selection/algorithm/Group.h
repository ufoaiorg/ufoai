#ifndef SELECTIONGROUP_H_
#define SELECTIONGROUP_H_

#include "iselection.h"
#include <list>

namespace selection {
	namespace algorithm {

	/**
	 * greebo: This selects all the children of an entity, given the case
	 *         that a child of this entity is already selected. For instance,
	 *         if a child brush of a func_static is selected, this command
	 *         expands the selection to all other children (but not the
	 *         func_static entity itself). Select a single primitive of
	 *         the worldspawn entity and this command will select every primitive
	 *         that is child of worldspawn.
	 */
	void expandSelectionToEntities();

	/** greebo: This re-parents the selected primitives to an entity. The entity has to
	 * 			be selected last. Emits an error message if the selection doesn't meet
	 * 			the requirements
	 */
	void parentSelection();

	/**
	 * Tests the current selection and returns true if the selection is suitable
	 * for reparenting the selected primitives to the (last) selected entity.
	 */
	bool curSelectionIsSuitableForReparent();

	} // namespace algorithm
} // namespace selection

#endif /*SELECTIONGROUP_H_*/
