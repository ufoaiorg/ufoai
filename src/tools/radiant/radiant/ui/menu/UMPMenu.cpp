#include "UMPMenu.h"

#include "radiant_i18n.h"
#include "ifilter.h"
#include "iuimanager.h"
#include "iump.h"
#include "iradiant.h"
#include "ifilesystem.h"

#include "string/string.h"
#include "os/path.h"
#include "gtkutil/pointer.h"
#include "gtkutil/container.h"

#include "../../ump/UMPFile.h"
#include "../../map/map.h"

namespace ui {
	namespace {
		const std::string MENU_NAME = "main";
		const std::string MENU_MAP_PATH = MENU_NAME + "/map";
		const std::string MENU_UMP_NAME = "ump";
		const std::string MENU_PATH = MENU_MAP_PATH + "/" + MENU_UMP_NAME;
		const std::string MENU_ICON = "";
	}

/**
 * Callback function for menu items to load the specific file
 * @param menuItem activated menu item
 * @param name map name to load as pointer to g_quark
 */
void UMPMenu::_activate (GtkMenuItem* menuItem, gpointer name)
{
	GlobalMap().changeMap(_("Open Map"), g_quark_to_string(gpointer_to_int(name)));
}

/**
 * visitor implementation that adds a menu item for each ump tile
 */
class UMPTileToMenuItemVisitor: public map::ump::UMPTileVisitor
{
	private:
		map::ump::UMPFile&_file;

	public:
		UMPTileToMenuItemVisitor (map::ump::UMPFile &file) :
			_file(file)
		{
		}
		void visit (const map::ump::UMPTile& tile) const
		{
			// Get the menu manager
			IMenuManager* menuManager = GlobalUIManager().getMenuManager();

			std::string base = _file.getBase();
			const std::string name = tile.getTileName(base);
			const std::string relativeName = "maps/" + _file.getMapName(name) + ".map";
			const std::string filename = GlobalFileSystem().findFile(relativeName) + relativeName;

			// Create the filter menu item
			GtkWidget* item = menuManager->add(MENU_PATH, os::getFilenameFromPath(filename),
						ui::menuItem, name,
						MENU_ICON, "");
			gtk_widget_set_sensitive(item, true);
			g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(UMPMenu::_activate), gint_to_pointer(
					g_quark_from_string(filename.c_str())));
		}
};

/**
 * Method recreates rma tile menu adding entries for all tiles in current rma if available
 */
void UMPMenu::addItems()
{
	IMenuManager* menuManager = GlobalUIManager().getMenuManager();
	menuManager->remove(MENU_PATH);

	std::string umpFilename = GlobalUMPSystem().getUMPFilename(GlobalMap().getName());
	if (umpFilename.empty())
		return;
	map::ump::UMPFile file = map::ump::UMPFile(umpFilename);
	if (!file.load())
		return;

	// Create the sub menu item in the map menu
	menuManager->add(MENU_MAP_PATH, MENU_UMP_NAME, ui::menuFolder, _("RMA tiles"), MENU_ICON, "");

	UMPTileToMenuItemVisitor visitor(file);
	file.forEachTile(visitor);
}

}
