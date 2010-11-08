#ifndef _SELECTION_SET_H_
#define _SELECTION_SET_H_

#include <set>

#include "iselectionset.h"
#include "iselection.h"
#include "iscenegraph.h"
#include "scenelib.h"

namespace selection
{

class SelectionSet :
	public ISelectionSet,
	public scene::EraseObserver
{
private:
	typedef std::set<scene::Instance*> NodeSet;
	NodeSet _nodes;

	std::string _name;

public:
	~SelectionSet();

	SelectionSet(const std::string& name);

	const std::string& getName();

	// Checks whether this set is empty
	bool empty();

	void onErase(scene::Instance *instance);

	// Clear members
	void clear();

	// Selects all member nodes of this set
	void select();

	// De-selects all member nodes of this set
	void deselect();

	void addNode(scene::Instance* instance);

	// Clears this set and loads the currently selected nodes in the
	// scene as new members into this set.
	void assignFromCurrentScene();
};
typedef SelectionSet* SelectionSetPtr;

} // namespace

#endif /* _SELECTION_SET_H_ */
