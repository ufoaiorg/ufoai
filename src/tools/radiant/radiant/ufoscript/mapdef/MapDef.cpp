#include "MapDef.h"
#include "../../map/map.h"
#include "radiant_i18n.h"
#include "iregistry.h"
#include "iump.h"
#include "gtkutil/dialog.h"
#include "../../environment.h"
#include "../../ui/scripteditor/UFOScriptEditor.h"
#include "os/path.h"
#include "stream/textfilestream.h"

namespace scripts
{
	MapDef::~MapDef ()
	{
	}

	MapDef::MapDef () :
		parser("mapdef")
	{
	}

	// TODO: A lot of these values can be taken from the map data
	bool MapDef::add ()
	{
		// already a mapdef for this map available?
		if (getMapDefForCurrentMap()) {
			showMapDefinition();
			return false;
		}

		TextFileInputStream file(GlobalRegistry().get(RKEY_APP_PATH) + "mapdef.template");
		if (!file.failed()) {
			ui::UFOScriptEditor editor("ufos/maps.ufo", file.getString());
			editor.show();
			return true;
		}

		const std::string mapDefID = getMapDefID();
		std::stringstream os;
		os << "mapdef " << mapDefID << std::endl;
		os << "{" << std::endl;
		os << "\tmap\t" << mapDefID << std::endl;
		os << "}" << std::endl;
		ui::UFOScriptEditor editor("ufos/maps.ufo", os.str());
		editor.show();

		return true;
	}

	std::string MapDef::getMapDefID ()
	{
		const std::string mapname = os::getFilenameFromPath(GlobalMap().getName());
		const std::string umpName = GlobalUMPSystem().getUMPFilename(mapname);
		if (umpName.length() > 0)
			return os::stripExtension(umpName);

		const std::string baseName = os::stripExtension(mapname);
		return baseName;
	}

	DataBlock* MapDef::getMapDefForCurrentMap ()
	{
		const std::string mapDefID = getMapDefID();
		return parser.getEntryForID(mapDefID);
	}

	void MapDef::showMapDefinition ()
	{
		if (GlobalMap().isUnnamed()) {
			gtkutil::infoDialog(_("Save your map before using this function"));
			return;
		}

		DataBlock* dataBlock = getMapDefForCurrentMap();
		if (dataBlock) {
			// found it, now show it
			ui::UFOScriptEditor editor("ufos/" + dataBlock->getFilename());
			editor.goToLine(dataBlock->getLineNumber());
			editor.show();
			return;
		}

		gtkutil::infoDialog(_("Could not find any associated map definition"));
	}
}
