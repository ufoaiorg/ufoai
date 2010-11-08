#ifndef _SELECTION_SET_MANAGER_H_
#define _SELECTION_SET_MANAGER_H_

#include "iselectionset.h"
#include "iradiant.h"

#include <map>
#include "SelectionSet.h"
#include "SelectionSetToolmenu.h"

namespace selection
{

class SelectionSetManager :
	public ISelectionSetManager
{
private:
	typedef std::set<ISelectionSetManager::Observer*> Observers;
	Observers _observers;

	SelectionSetToolmenuPtr _toolmenu;

	typedef std::map<std::string, SelectionSetPtr> SelectionSets;
	SelectionSets _selectionSets;

public:
	typedef ISelectionSetManager Type;
	STRING_CONSTANT(Name, "*");

	ISelectionSetManager* getTable() {
		return this;
	}

	SelectionSetManager();
	~SelectionSetManager();

	void init(GtkToolbar* toolbar);

	// ISelectionManager implementation
	void addObserver(Observer& observer);
	void removeObserver(Observer& observer);

	void foreachSelectionSet(Visitor& visitor);
	ISelectionSetPtr createSelectionSet(const std::string& name);
	void deleteSelectionSet(const std::string& name);
	void deleteAllSelectionSets();
	ISelectionSetPtr findSelectionSet(const std::string& name);

private:
	void notifyObservers();
};

} // namespace

#endif /* _SELECTION_SET_MANAGER_H_ */
