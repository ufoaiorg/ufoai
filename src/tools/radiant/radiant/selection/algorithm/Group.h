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

	} // namespace algorithm
} // namespace selection

#endif /*SELECTIONGROUP_H_*/
