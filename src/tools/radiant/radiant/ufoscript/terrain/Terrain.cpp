#include "Terrain.h"
#include "radiant_i18n.h"
#include "iradiant.h"
#include "iselection.h"
#include "gtkutil/dialog.h"
#include "../../ui/scripteditor/UFOScriptEditor.h"
#include "../../brush/BrushVisit.h"
#include "../../brush/Face.h"
#include "../../brush/FaceInstance.h"

#include <string>
#include <map>

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

	namespace
	{
		static const std::string& TEXTURES = "textures/";
		static const std::size_t TEXTURES_LENGTH = TEXTURES.length();

		// Functor to add terrain definition to a string stream that is later
		// saved to the terrain.ufo script file
		class GenerateTerrainForFaces
		{
			private:

				std::stringstream& _os;
				Terrain* _terrain;

			private:

				/**
				 * @param terrainID The id that is used in the scripts. In general
				 * this is the texture path relative to textures/
				 * @return @c true if the terrain definition was already defined
				 * in this run, or it is a special texture that is not shown in
				 * the game and thus needs no terrain definition.
				 */
				bool skipTexture (const std::string& terrainID) const
				{
					if (_os.str().find(terrainID) != std::string::npos)
						return true;

					if (terrainID.find("tex_common/") != std::string::npos)
						return true;

					return false;
				}

			public:
				GenerateTerrainForFaces (std::stringstream& os, Terrain* terrain) :
					_os(os), _terrain(terrain)
				{
				}

				void operator() (Face& face) const
				{
					const std::string texture = face.GetShader();
					std::string terrainID = texture.substr(TEXTURES_LENGTH);
					if (skipTexture(terrainID))
						return;

					const DataBlock* dataBlock = _terrain->getTerrainDefitionForTexture(texture);
					if (!dataBlock) {
						_os << "terrain " << terrainID << std::endl;
						_os << "{" << std::endl;
						_os << "//\tfootstepsound\t\"footsteps/grass2\"" << std::endl;
						_os << "//\tbouncefraction\t1.0" << std::endl;
						_os << "}" << std::endl;
					}
				}
		};
	}

	void Terrain::generateTerrainDefinitionsForTextures ()
	{
		if (!GlobalSelectionSystem().areFacesSelected()) {
			gtkutil::infoDialog(_("No faces selected"));
			return;
		}

		std::stringstream os;
		Scene_ForEachSelectedBrushFace(GenerateTerrainForFaces(os, this));
		const std::string& newTerrainDefinitions = os.str();
		if (newTerrainDefinitions.empty()) {
			showTerrainDefinitionForTexture();
		} else {
			ui::UFOScriptEditor editor("ufos/terrain.ufo", newTerrainDefinitions);
			editor.show();
		}
	}

	const DataBlock* Terrain::getTerrainDefitionForTexture (const std::string& texture)
	{
		for (Parser::EntriesIterator i = _blocks.begin(); i != _blocks.end(); ++i) {
			const DataBlock* blockData = (*i);
			const std::string terrainID = TEXTURES + blockData->getID();
			if (terrainID == texture)
				return blockData;
		}
		return (DataBlock*) 0;
	}

	void Terrain::showTerrainDefinitionForTexture ()
	{
		if (!GlobalSelectionSystem().areFacesSelected()) {
			gtkutil::infoDialog(_("No faces selected"));
			return;
		}

		const Face &face = g_SelectedFaceInstances.last().getFace();
		const std::string shader = face.GetShader();

		const DataBlock* blockData = getTerrainDefitionForTexture(shader);
		if (blockData) {
			// found it, now show it
			ui::UFOScriptEditor editor("ufos/" + blockData->getFilename());
			editor.goToLine(blockData->getLineNumber());
			editor.show();
			return;
		}

		gtkutil::infoDialog(_("Could not find any associated terrain definition"));
	}
}
