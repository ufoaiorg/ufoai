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

#if !defined(INCLUDED_IFILTER_H)
#define INCLUDED_IFILTER_H

#include "generic/constant.h"
#include <string>

enum {
	EXCLUDE_WORLD               = 0x00000001,
	EXCLUDE_ENT                 = 0x00000002,
	EXCLUDE_TRANSLUCENT         = 0x00000004,
	EXCLUDE_LIQUIDS             = 0x00000008,
	EXCLUDE_CAULK               = 0x00000010,
	EXCLUDE_CLIP                = 0x00000020,
	EXCLUDE_ACTORCLIP           = 0x00000040,
	EXCLUDE_WEAPONCLIP          = 0x00000080,
	EXCLUDE_LIGHTS              = 0x00000100,
	EXCLUDE_DETAILS             = 0x00000200,
	EXCLUDE_HINTSSKIPS          = 0x00000400,
	EXCLUDE_MODELS              = 0x00000800,
	EXCLUDE_TRIGGERS            = 0x00001000,
	EXCLUDE_TERRAIN             = 0x00002000,
	EXCLUDE_NODRAW              = 0x00004000,
	EXCLUDE_STRUCTURAL          = 0x00008000,
	EXCLUDE_NO_SURFLIGHTS       = 0x00010000,
	EXCLUDE_PHONG               = 0x00020000,
	EXCLUDE_NO_FOOTSTEPS        = 0x00040000,
	EXCLUDE_PARTICLE            = 0x00080000,
	EXCLUDE_INFO_PLAYER_START   = 0x00100000,
	EXCLUDE_INFO_HUMAN_START    = 0x00200000,
	EXCLUDE_INFO_ALIEN_START    = 0x00400000,
	EXCLUDE_INFO_2x2_START      = 0x00800000,
	EXCLUDE_INFO_CIVILIAN_START = 0x01000000,
	EXCLUDE_INFO                = 0x02000000
};

class Filter {
public:
	virtual ~Filter(){}
	virtual void setActive(bool active) = 0;
};

/**
* Interface for objects which can be filtered by the FilterSystem.
*/
class Filterable {
public:
	virtual ~Filterable(){}
	virtual void updateFiltered() = 0;
};

/** Visitor interface for evaluating the available filters in the
 * FilterSystem.
 */
struct IFilterVisitor {
	// Visit function
	virtual void visit(const std::string& filterName) = 0;
};

/** Interface for the FilterSystem.
 */
class FilterSystem
{
public:
	INTEGER_CONSTANT(Version, 1);
	STRING_CONSTANT(Name, "filters");

	/** Visit the available filters, passing each filter's text
	 * name to the visitor.
	 *
	 * @param visitor
	 * Visitor class implementing the IFilterVisitor interface.
	 */
	virtual void forEachFilter(IFilterVisitor& visitor) = 0;

	/** Set the state of the named filter.
	 *
	 * @param filter
	 * The filter to toggle.
	 *
	 * @param state
	 * true if the filter should be active, false otherwise.
	 */
	virtual void setFilterState(const std::string& filter, bool state) = 0;

	/** Test if a given item should be visible or not, based on the currently-
	 * active filters.
	 *
	 * @param item
	 * The item to query - "texture", "entityclass", "surfaceflags" or "contentflags"
	 *
	 * @param text
	 * String name of the item to query.
	 *
	 * @returns
	 * true if the item is visible, false otherwise.
	 */
	virtual bool isVisible(const std::string& item, const std::string& text) = 0;
	virtual bool isVisible(const std::string& item, int flags) = 0;

	virtual ~FilterSystem(){}
	virtual void addFilter(Filter& filter, int mask) = 0;
	virtual void registerFilterable(Filterable& filterable) = 0;
	virtual void unregisterFilterable(Filterable& filterable) = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<FilterSystem> GlobalFilterModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<FilterSystem> GlobalFilterModuleRef;

inline FilterSystem& GlobalFilterSystem() {
	return GlobalFilterModule::getTable();
}

#endif
