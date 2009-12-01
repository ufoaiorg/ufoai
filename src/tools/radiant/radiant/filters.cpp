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

class BasicFilterSystem: public FilterSystem
{
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

void Filters_constructMenu (GtkMenu* menu_in_menu)
{
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "World"), "FilterWorldBrushes");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Entities"), "FilterEntities");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Translucent"), "FilterTranslucent");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Liquids"), "FilterLiquids");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Caulk"), "FilterCaulk");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Clips"), "FilterClips");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "ActorClips"), "FilterActorClips");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "WeaponClips"), "FilterWeaponClips");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Lights"), "FilterLights");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "NoSurfLights"), "FilterNoSurfLights");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "NoFootsteps"), "FilterNoFootsteps");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Structural"), "FilterStructural");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Nodraw"), "FilterNodraw");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Phong"), "FilterPhong");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Details"), "FilterDetails");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Hints"), "FilterHintsSkips");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Models"), "FilterModels");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Triggers"), "FilterTriggers");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Particles"), "FilterParticles");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Info"), "FilterInfo");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Info player start"), "FilterInfoPlayerStart");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Info human start"), "FilterInfoHumanStart");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Info 2x2 start"), "FilterInfo2x2Start");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Info alien start"), "FilterInfoAlienStart");
	create_check_menu_item_with_mnemonic(menu_in_menu, C_("Filter Menu", "Info civilian start"), "FilterInfoCivilianStart");

	// filter manipulation
	menu_separator(menu_in_menu);
	create_menu_item_with_mnemonic(menu_in_menu, _("Invert filters"), "InvertFilters");
	create_menu_item_with_mnemonic(menu_in_menu, _("Reset filters"), "ResetFilters");
}

#include "preferencesystem.h"
#include "stringio.h"

void ConstructFilters ()
{
	GlobalPreferenceSystem().registerPreference("SI_Exclude", SizeImportStringCaller(g_filters_globals.exclude),
			SizeExportStringCaller(g_filters_globals.exclude));

	GlobalCommands_insert("InvertFilters", FreeCaller<InvertFilters> ());
	GlobalCommands_insert("ResetFilters", FreeCaller<ResetFilters> ());

	add_filter_command(EXCLUDE_WORLD, "FilterWorldBrushes", Accelerator('1', (GdkModifierType) GDK_MOD1_MASK));
	add_filter_command(EXCLUDE_ENT, "FilterEntities", Accelerator('2', (GdkModifierType) GDK_MOD1_MASK));
	add_filter_command(EXCLUDE_TRANSLUCENT, "FilterTranslucent", Accelerator('3', (GdkModifierType) GDK_MOD1_MASK));
	add_filter_command(EXCLUDE_LIQUIDS, "FilterLiquids", Accelerator('4', (GdkModifierType) GDK_MOD1_MASK));
	add_filter_command(EXCLUDE_CAULK, "FilterCaulk", Accelerator('5', (GdkModifierType) GDK_MOD1_MASK));
	add_filter_command(EXCLUDE_CLIP, "FilterClips", Accelerator('6', (GdkModifierType) GDK_MOD1_MASK));
	add_filter_command(EXCLUDE_ACTORCLIP, "FilterActorClips", Accelerator('7', (GdkModifierType) GDK_MOD1_MASK));
	add_filter_command(EXCLUDE_WEAPONCLIP, "FilterWeaponClips", Accelerator('8', (GdkModifierType) GDK_MOD1_MASK));
	add_filter_command(EXCLUDE_LIGHTS, "FilterLights", Accelerator('9', (GdkModifierType) GDK_MOD1_MASK));
	add_filter_command(EXCLUDE_NO_SURFLIGHTS, "FilterNoSurfLights", Accelerator('9', (GdkModifierType) (GDK_MOD1_MASK
			| GDK_CONTROL_MASK)));
	add_filter_command(EXCLUDE_STRUCTURAL, "FilterStructural", Accelerator('0', (GdkModifierType) (GDK_SHIFT_MASK
			| GDK_CONTROL_MASK)));
	add_filter_command(EXCLUDE_NODRAW, "FilterNodraw", Accelerator('N', (GdkModifierType) GDK_CONTROL_MASK));
	add_filter_command(EXCLUDE_DETAILS, "FilterDetails", Accelerator('D', (GdkModifierType) GDK_CONTROL_MASK));
	add_filter_command(EXCLUDE_HINTSSKIPS, "FilterHintsSkips", Accelerator('H', (GdkModifierType) GDK_CONTROL_MASK));
	add_filter_command(EXCLUDE_MODELS, "FilterModels", Accelerator('M', (GdkModifierType) GDK_SHIFT_MASK));
	add_filter_command(EXCLUDE_TRIGGERS, "FilterTriggers", Accelerator('T', (GdkModifierType) (GDK_SHIFT_MASK
			| GDK_CONTROL_MASK)));
	add_filter_command(EXCLUDE_PHONG, "FilterPhong", Accelerator('P', (GdkModifierType) (GDK_SHIFT_MASK
			| GDK_CONTROL_MASK)));
	add_filter_command(EXCLUDE_NO_FOOTSTEPS, "FilterNoFootsteps", Accelerator('F', (GdkModifierType) (GDK_SHIFT_MASK
			| GDK_CONTROL_MASK)));

	add_filter_command(EXCLUDE_PARTICLE, "FilterParticles", accelerator_null());
	add_filter_command(EXCLUDE_INFO_2x2_START, "FilterInfo2x2Start", accelerator_null());
	add_filter_command(EXCLUDE_INFO_ALIEN_START, "FilterInfoAlienStart", accelerator_null());
	add_filter_command(EXCLUDE_INFO_CIVILIAN_START, "FilterInfoCivilianStart", accelerator_null());
	add_filter_command(EXCLUDE_INFO_HUMAN_START, "FilterInfoHumanStart", accelerator_null());
	add_filter_command(EXCLUDE_INFO_PLAYER_START, "FilterInfoPlayerStart", accelerator_null());
	add_filter_command(EXCLUDE_INFO, "FilterInfo", accelerator_null());

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
