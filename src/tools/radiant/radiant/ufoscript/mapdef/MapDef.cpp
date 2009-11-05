#include "MapDef.h"
#include "../../map/map.h"
#include "radiant_i18n.h"
#include "iradiant.h"
#include "gtkutil/dialog.h"
#include "../../brush/brush.h"
#include "../../ui/scripteditor/UFOScriptEditor.h"
#include "os/path.h"

namespace scripts
{
	MapDef::~MapDef ()
	{
	}

	MapDef::MapDef () :
		parser("mapdef")
	{
		_blocks = parser.getEntries();
	}

	bool MapDef::add ()
	{
		return false;
	}

	void MapDef::showMapDefinition ()
	{
		const std::string mapname = os::getFilenameFromPath(GlobalRadiant().getMapName());
		if (mapname.empty() || Map_Unnamed(g_map)) {
			gtkutil::infoDialog(GlobalRadiant().getMainWindow(), _("Save your map before using this function"));
			return;
		}

		const std::string baseName = os::stripExtension(mapname);
		for (Parser::EntriesIterator i = _blocks.begin(); i != _blocks.end(); i++) {
			DataBlock* blockData = (*i);
			if (baseName == blockData->getID()) {
				// found it, now show it
				ui::UFOScriptEditor editor("ufos/" + blockData->getFilename());
				editor.goToLine(blockData->getLineNumber());
				editor.show();
				return;
			}
		}

		gtkutil::infoDialog(GlobalRadiant().getMainWindow(), _("Could not find any associated map definition"));
	}
}
