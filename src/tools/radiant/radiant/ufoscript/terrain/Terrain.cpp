#include "Terrain.h"
#include "radiant_i18n.h"
#include "iradiant.h"
#include "iselection.h"
#include "gtkutil/dialog.h"
#include "../../brush/brush.h"
#include "../../ui/scripteditor/UFOScriptEditor.h"

namespace scripts
{
	Terrain::Terrain () :
		parser("terrain")
	{
		_blocks = parser.getEntries();
	}

	Terrain::~Terrain ()
	{
	}

	void Terrain::showTerrainDefinitionForTexture ()
	{
		if (g_SelectedFaceInstances.empty()) {
			gtkutil::infoDialog(GlobalRadiant().getMainWindow(), _("No faces selected"));
			return;
		}

		const Face &face = g_SelectedFaceInstances.last().getFace();
		for (Parser::EntriesIterator i = _blocks.begin(); i != _blocks.end(); i++) {
			DataBlock* blockData = (*i);
			const std::string shader = face.GetShader();
			const std::string terrainID = "textures/" + blockData->getID();
			if (terrainID == shader) {
				// found it, now show it
				ui::UFOScriptEditor editor("ufos/" + blockData->getFilename());
				editor.goToLine(blockData->getLineNumber());
				editor.show();
				return;
			}
		}

		gtkutil::infoDialog(GlobalRadiant().getMainWindow(), _("Could not find any associated terrain definition"));
	}
}
