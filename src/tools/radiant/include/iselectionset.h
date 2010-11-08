#ifndef _ISELECTION_SET_H_
#define _ISELECTION_SET_H_

#include "iradiant.h"

typedef struct _GtkToolbar GtkToolbar;

namespace selection
{

class ISelectionSet
{
public:

	virtual ~ISelectionSet() {}

	// The name of this set
	virtual const std::string& getName() = 0;

	// Checks whether this set is empty
	virtual bool empty() = 0;

	// Selects all member nodes of this set
	virtual void select() = 0;

	// De-selects all member nodes of this set
	virtual void deselect() = 0;

	// Removes all members, leaving this set as empty
	virtual void clear() = 0;

	// Clears this set and loads the currently selected nodes in the
	// scene as new members into this set.
	virtual void assignFromCurrentScene() = 0;
};
typedef ISelectionSet* ISelectionSetPtr;

class ISelectionSetManager
{
public:
	INTEGER_CONSTANT(Version, 1);
	STRING_CONSTANT(Name, "selectionset");

	virtual ~ISelectionSetManager() {}

	class Observer
	{
	public:
		virtual ~Observer() {}

		// Called when the list of selection sets has been changed,
		// by deletion or addition
		virtual void onSelectionSetsChanged() = 0;
	};

	virtual void addObserver(Observer& observer) = 0;
	virtual void removeObserver(Observer& observer) = 0;

	class Visitor
	{
	public:
		virtual ~Visitor() {}

		virtual void visit(const ISelectionSetPtr& set) = 0;
	};

	virtual void init(GtkToolbar* toolbar) = 0;

	/**
	 * greebo: Traverses the list of selection sets using
	 * the given visitor class.
	 */
	virtual void foreachSelectionSet(Visitor& visitor) = 0;

	/**
	 * greebo: Creates a new selection set with the given name.
	 * If a selection with that name is already registered, the existing
	 * one is returned.
	 */
	virtual ISelectionSetPtr createSelectionSet(const std::string& name) = 0;

	/**
	 * Removes the named selection set. If the named set is
	 * not existing, nothing happens.
	 */
	virtual void deleteSelectionSet(const std::string& name) = 0;

	/**
	 * Deletes all sets.
	 */
	virtual void deleteAllSelectionSets() = 0;

	/**
	 * Finds the named selection set.
	 *
	 * @returns the found selection set or NULL if the set is not existent.
	 */
	virtual ISelectionSetPtr findSelectionSet(const std::string& name) = 0;
};

} // namespace

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<selection::ISelectionSetManager> GlobalSelectionSetManagerModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<selection::ISelectionSetManager> GlobalSelectionSetManagerModuleRef;

inline selection::ISelectionSetManager& GlobalSelectionSetManager ()
{
	return GlobalSelectionSetManagerModule::getTable();
}

#endif /* _ISELECTION_SET_H_ */
