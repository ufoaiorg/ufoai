#include "SelectionSetManager.h"

#include "itextstream.h"
#include "iradiant.h"
#include "iselection.h"
#include "ieventmanager.h"

#include <gtk/gtktoolbar.h>
#include <gtk/gtkseparatortoolitem.h>

namespace selection
{

SelectionSetManager::~SelectionSetManager()
{
	for (SelectionSets::const_iterator i = _selectionSets.begin(); i != _selectionSets.end(); ++i) {
		delete i->second;
	}

	if (_toolmenu)
		delete _toolmenu;
}

SelectionSetManager::SelectionSetManager()
{
}

// the horizontal toolbar to add a custom widget
void SelectionSetManager::init(GtkToolbar* toolbar) {
	// Insert a separator at the end of the toolbar
	GtkToolItem* item = GTK_TOOL_ITEM(gtk_separator_tool_item_new());
	gtk_toolbar_insert(toolbar, item, -1);

	gtk_widget_show(GTK_WIDGET(item));

	// Construct a new tool menu object
	_toolmenu = new SelectionSetToolmenu;

	gtk_toolbar_insert(toolbar, _toolmenu->getToolItem(), -1);
}

void SelectionSetManager::addObserver(Observer& observer)
{
	_observers.insert(&observer);
}

void SelectionSetManager::removeObserver(Observer& observer)
{
	_observers.erase(&observer);
}

void SelectionSetManager::notifyObservers()
{
	for (Observers::iterator i = _observers.begin(); i != _observers.end(); )
	{
		(*i++)->onSelectionSetsChanged();
	}
}

void SelectionSetManager::foreachSelectionSet(Visitor& visitor)
{
	for (SelectionSets::const_iterator i = _selectionSets.begin(); i != _selectionSets.end(); )
	{
		visitor.visit((i++)->second);
	}
}

ISelectionSetPtr SelectionSetManager::createSelectionSet(const std::string& name)
{
	SelectionSets::iterator i = _selectionSets.find(name);

	if (i == _selectionSets.end())
	{
		// Create new set
		std::pair<SelectionSets::iterator, bool> result = _selectionSets.insert(
			SelectionSets::value_type(name, new SelectionSet(name)));

		i = result.first;

		notifyObservers();
	}

	return i->second;
}

void SelectionSetManager::deleteSelectionSet(const std::string& name)
{
	SelectionSets::iterator i = _selectionSets.find(name);

	if (i != _selectionSets.end())
	{
		_selectionSets.erase(i);

		notifyObservers();
	}
}

void SelectionSetManager::deleteAllSelectionSets()
{
	for (SelectionSets::iterator i = _selectionSets.begin(); i != _selectionSets.end(); ++i) {
		delete i->second;
	}
	_selectionSets.clear();
	notifyObservers();
}

ISelectionSetPtr SelectionSetManager::findSelectionSet(const std::string& name)
{
	SelectionSets::iterator i = _selectionSets.find(name);

	return (i != _selectionSets.end()) ? i->second : NULL;
}

} // namespace

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

// Define the static SelectionSetManager module
class SelectionSetManagerDependencies :
	public GlobalEventManagerModuleRef,
	public GlobalRadiantModuleRef
{};

typedef SingletonModule<selection::SelectionSetManager, SelectionSetManagerDependencies> SelectionSetManagerModule;
typedef Static<SelectionSetManagerModule> StaticSelectionSetManagerModule;
StaticRegisterModule staticRegisterSelectionSetManager(StaticSelectionSetManagerModule::instance());
