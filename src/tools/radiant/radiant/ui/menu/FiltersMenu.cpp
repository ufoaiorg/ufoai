#include "FiltersMenu.h"

#include "radiant_i18n.h"
#include "ifilter.h"
#include "gtkutil/IconTextMenuToggle.h"
#include "gtkutil/menu.h"

#include <gtk/gtkmenu.h>
#include "../../gtkmisc.h"

namespace ui
{

// Construct GTK widgets

FiltersMenu::FiltersMenu()
: _menu(new_sub_menu_item_with_mnemonic(_("Filter")))
{
	// Local visitor class to populate the menu
	struct MenuPopulatingVisitor
	: public IFilterVisitor
	{
		// FiltersMenu to populate
		GtkMenu* _menu;

		// Constructor
		MenuPopulatingVisitor(GtkMenu* menu)
		: _menu(menu)
		{}

		// Visitor function
		void visit(const std::string& filterName) {
			// Create and append the menu item for this filter
			GtkWidget* filterItem = gtkutil::IconTextMenuToggle("iconFilter16.png", filterName);
			gtk_widget_show_all(filterItem);
			gtk_menu_shell_append(GTK_MENU_SHELL(_menu), filterItem);
			// Connect up the signal
			g_signal_connect(G_OBJECT(filterItem),
							 "toggled",
							 G_CALLBACK(_onFilterToggle),
							 const_cast<std::string*>(&filterName)); // string owned by filtersystem, GTK requires void* not const void*
		}
	};

	// Visit the filters in the FilterSystem to populate the menu
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(_menu));
	MenuPopulatingVisitor visitor(menu);
	GlobalFilterSystem().forEachFilter(visitor);

	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "World"), "FilterWorldBrushes");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Entities"), "FilterEntities");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Translucent"), "FilterTranslucent");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Liquids"), "FilterLiquids");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Caulk"), "FilterCaulk");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Clips"), "FilterClips");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "ActorClips"), "FilterActorClips");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "WeaponClips"), "FilterWeaponClips");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Lights"), "FilterLights");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "NoSurfLights"), "FilterNoSurfLights");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "NoFootsteps"), "FilterNoFootsteps");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Structural"), "FilterStructural");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Nodraw"), "FilterNodraw");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Phong"), "FilterPhong");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Details"), "FilterDetails");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Hints"), "FilterHintsSkips");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Models"), "FilterModels");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Triggers"), "FilterTriggers");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Particles"), "FilterParticles");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Info"), "FilterInfo");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Info player start"), "FilterInfoPlayerStart");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Info human start"), "FilterInfoHumanStart");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Info 2x2 start"), "FilterInfo2x2Start");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Info alien start"), "FilterInfoAlienStart");
	create_check_menu_item_with_mnemonic(menu, C_("Filter Menu", "Info civilian start"), "FilterInfoCivilianStart");

	// filter manipulation
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("Invert filters"), "InvertFilters");
	create_menu_item_with_mnemonic(menu, _("Reset filters"), "ResetFilters");
}

/* GTK CALLBACKS */

void FiltersMenu::_onFilterToggle(GtkCheckMenuItem* item, const std::string* name) {
	GlobalFilterSystem().setFilterState(*name, gtk_check_menu_item_get_active(item));
}

} // namespace ui
