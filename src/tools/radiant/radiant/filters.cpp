/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "filters.h"
#include "radiant_i18n.h"

#include "ifilter.h"

#include "scenelib.h"

#include <list>
#include <set>

#include "gtkutil/widget.h"
#include "gtkutil/menu.h"
#include "gtkmisc.h"
#include "mainframe.h"
#include "commands.h"
#include "settings/preferences.h"
#include "iregistry.h"
#include "iscenegraph.h"


struct filters_globals_t
{
		std::size_t exclude;

		filters_globals_t () :
			exclude(0)
		{
		}
};

filters_globals_t g_filters_globals;

static inline bool filter_active (int mask)
{
	return (g_filters_globals.exclude & mask) > 0;
}

class FilterWrapper
{
	public:
		FilterWrapper (Filter& filter, int mask) :
			m_filter(filter), m_mask(mask)
		{
		}
		void update ()
		{
			m_filter.setActive(filter_active(m_mask));
		}
	private:
		Filter& m_filter;
		int m_mask;
};

typedef std::list<FilterWrapper> Filters;
static Filters g_filters;

typedef std::set<Filterable*> Filterables;
static Filterables g_filterables;

void UpdateFilters ()
{
	for (Filters::iterator i = g_filters.begin(); i != g_filters.end(); ++i) {
		(*i).update();
	}
	for (Filterables::iterator i = g_filterables.begin(); i != g_filterables.end(); ++i) {
		(*i)->updateFiltered();
	}
}

#include "filters/XMLFilter.h"

/** FilterSystem implementation class.
 */
class BasicFilterSystem
: public FilterSystem
{
private:

	// Flag to indicate initialisation status
	bool _initialised;

	// Hashtable of available filters, indexed by name
	typedef std::map<std::string, filters::XMLFilter> FilterTable;
	FilterTable _availableFilters;

	// Second table containing just the active filters
	FilterTable _activeFilters;

	// Cache of visibility flags for item names, to avoid having to
	// traverse the active filter list for each lookup
	typedef std::map<std::string, bool> StringFlagCache;
	StringFlagCache _visibilityCache;

private:

	// Initialise the filter system. This must be done after the main
	// Radiant module, hence cannot be done in the constructor.
	void initialise() {

		// Ask the XML Registry for the filter nodes
		xml::NodeList filters = GlobalRegistry().findXPath("game/filtersystem//filter");

		// Iterate over the list of nodes, adding filter objects onto the list
		for (xml::NodeList::iterator iter = filters.begin();
			 iter != filters.end();
			 ++iter)
		{
			// Initialise the XMLFilter object
			std::string filterName = iter->getAttributeValue("name");
			filters::XMLFilter filter(filterName);

			// Get all of the filterCriterion children of this node
			xml::NodeList critNodes = iter->getNamedChildren("filterCriterion");

			// Create XMLFilterRule objects for each criterion
			for (xml::NodeList::iterator critIter = critNodes.begin();
				 critIter != critNodes.end();
				 ++critIter)
			{
				filter.addRule(critIter->getAttributeValue("type"),
						critIter->getAttributeValue("match"),
						critIter->getAttributeValue("action") == "show");
			}

			// Add this XMLFilter to the list of available filters
			_availableFilters.insert(FilterTable::value_type(filterName, filter));
		}
	}

public:

	// Constructor
	BasicFilterSystem()
	: _initialised(false)
	{}

	// Filter system visit function
	void forEachFilter(IFilterVisitor& visitor) {
		// Initialise the filter system if not already
		if (!_initialised)
			initialise();

		// Visit each filter on the list, passing the name to the visitor
		for (FilterTable::iterator iter = _availableFilters.begin();
			iter != _availableFilters.end();
			++iter)
		{
			visitor.visit(iter->first);
		}
	}

	// Set the state of a filter
	void setFilterState(const std::string& filter, bool state) {
		if (state) {
			// Copy the filter to the active filters list
			_activeFilters.insert(
					FilterTable::value_type(
					filter, _availableFilters.find(filter)->second));
		}
		else {
			// Remove filter from active filters list
			_activeFilters.erase(filter);
		}

		// Invalidate the visibility cache to force new values to be
		// loaded from the filters themselves
		_visibilityCache.clear();

		// Trigger an immediate scene redraw
		GlobalSceneGraph().sceneChanged();
	}

	// Query whether a item is visible or filtered out
	bool isVisible(const std::string& item, const std::string& name) {

		// Check if this texture is in the texture flag cache, returning
		// its cached value if found
		StringFlagCache::iterator cacheIter = _visibilityCache.find(name);
		if (cacheIter != _visibilityCache.end())
			return cacheIter->second;

		// Otherwise, walk the list of active filters to find a value for
		// this item.
		bool visFlag = true; // default if no filters modify it

		for (FilterTable::iterator activeIter = _activeFilters.begin();
			 activeIter != _activeFilters.end();
			 ++activeIter)
		{
			// Delegate the check to the filter object. If a filter returns
			// false for the visibility check, then the item is filtered
			// and we don't need any more checks.
			if (!activeIter->second.isVisible(item, name)) {
				visFlag = false;
				break;
			}
		}

		// Cache the result and return to caller
		_visibilityCache.insert(StringFlagCache::value_type(name, visFlag));
		return visFlag;
	}

	bool isVisible(const std::string& item, int flags) {
		return isVisible(item, string::toString(flags));
	}

	/* Legacy stuff */

	public:
		void addFilter (Filter& filter, int mask)
		{
			g_filters.push_back(FilterWrapper(filter, mask));
			g_filters.back().update();
		}
		void registerFilterable (Filterable& filterable)
		{
			ASSERT_MESSAGE(g_filterables.find(&filterable) == g_filterables.end(), "filterable already registered");
			filterable.updateFiltered();
			g_filterables.insert(&filterable);
		}
		void unregisterFilterable (Filterable& filterable)
		{
			ASSERT_MESSAGE(g_filterables.find(&filterable) != g_filterables.end(), "filterable not registered");
			g_filterables.erase(&filterable);
		}
};

BasicFilterSystem g_FilterSystem;

FilterSystem& GetFilterSystem ()
{
	return g_FilterSystem;
}

static void PerformFiltering ()
{
	UpdateFilters();
	SceneChangeNotify();
}

class ToggleFilterFlag
{
		const unsigned int m_mask;
	public:
		ToggleItem m_item;

		ToggleFilterFlag (unsigned int mask) :
			m_mask(mask), m_item(ActiveCaller(*this))
		{
		}
		ToggleFilterFlag (const ToggleFilterFlag& other) :
			m_mask(other.m_mask), m_item(ActiveCaller(*this))
		{
		}
		void active (const BoolImportCallback& importCallback)
		{
			importCallback((g_filters_globals.exclude & m_mask) != 0);
		}
		typedef MemberCaller1<ToggleFilterFlag, const BoolImportCallback&, &ToggleFilterFlag::active> ActiveCaller;
		void toggle ()
		{
			g_filters_globals.exclude ^= m_mask;
			m_item.update();
			PerformFiltering();
		}
		void reset ()
		{
			g_filters_globals.exclude = 0;
			m_item.update();
			PerformFiltering();
		}
		typedef MemberCaller<ToggleFilterFlag, &ToggleFilterFlag::toggle> ToggleCaller;
};

typedef std::list<ToggleFilterFlag> ToggleFilterFlags;
ToggleFilterFlags g_filter_items;

static void add_filter_command (unsigned int flag, const std::string& command, const Accelerator& accelerator)
{
	g_filter_items.push_back(ToggleFilterFlag(flag));
	GlobalToggles_insert(command, ToggleFilterFlag::ToggleCaller(g_filter_items.back()), ToggleItem::AddCallbackCaller(
			g_filter_items.back().m_item), accelerator);
}

void InvertFilters ()
{
	std::list<ToggleFilterFlag>::iterator iter;

	for (iter = g_filter_items.begin(); iter != g_filter_items.end(); ++iter) {
		iter->toggle();
	}
}

void ResetFilters ()
{
	std::list<ToggleFilterFlag>::iterator iter;

	for (iter = g_filter_items.begin(); iter != g_filter_items.end(); ++iter) {
		iter->reset();
	}
}

#include "preferencesystem.h"
#include "stringio.h"

void ConstructFilters ()
{
	GlobalPreferenceSystem().registerPreference("SI_Exclude", SizeImportStringCaller(g_filters_globals.exclude),
			SizeExportStringCaller(g_filters_globals.exclude));

	GlobalCommands_insert("InvertFilters", FreeCaller<InvertFilters> ());
	GlobalCommands_insert("ResetFilters", FreeCaller<ResetFilters> ());

	add_filter_command(EXCLUDE_TRANSLUCENT, "FilterTranslucent", Accelerator('3', (GdkModifierType) GDK_MOD1_MASK));
	add_filter_command(EXCLUDE_NO_SURFLIGHTS, "FilterNoSurfLights", Accelerator('9', (GdkModifierType) (GDK_MOD1_MASK
			| GDK_CONTROL_MASK)));
	add_filter_command(EXCLUDE_STRUCTURAL, "FilterStructural", Accelerator('0', (GdkModifierType) (GDK_SHIFT_MASK
			| GDK_CONTROL_MASK)));
	add_filter_command(EXCLUDE_DETAILS, "FilterDetails", Accelerator('D', (GdkModifierType) GDK_CONTROL_MASK));
	add_filter_command(EXCLUDE_HINTSSKIPS, "FilterHintsSkips", Accelerator('H', (GdkModifierType) GDK_CONTROL_MASK));
	add_filter_command(EXCLUDE_PHONG, "FilterPhong", Accelerator('P', (GdkModifierType) (GDK_SHIFT_MASK
			| GDK_CONTROL_MASK)));
	add_filter_command(EXCLUDE_NO_FOOTSTEPS, "FilterNoFootsteps", Accelerator('F', (GdkModifierType) (GDK_SHIFT_MASK
			| GDK_CONTROL_MASK)));

	PerformFiltering();
}

void DestroyFilters ()
{
	g_filters.clear();
}

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class FilterAPI
{
		FilterSystem* m_filter;
	public:
		typedef FilterSystem Type;
		STRING_CONSTANT(Name, "*");

		FilterAPI ()
		{
			ConstructFilters();

			m_filter = &GetFilterSystem();
		}
		~FilterAPI ()
		{
			DestroyFilters();
		}
		FilterSystem* getTable ()
		{
			return m_filter;
		}
};

typedef SingletonModule<FilterAPI> FilterModule;
typedef Static<FilterModule> StaticFilterModule;
StaticRegisterModule staticRegisterFilter(StaticFilterModule::instance());
