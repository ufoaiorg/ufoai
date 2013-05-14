#pragma once

#include "../common/Parser.h"

namespace scripts
{
	class Terrain
	{
		private:

			// the parser object that holds all the block data
			Parser parser;

			// the blocks with the data - destroyed with the parser
			std::vector<DataBlock*> _blocks;

		public:
			Terrain ();

			virtual ~Terrain ();

			// creates a terrain definition for every selected texture
			void generateTerrainDefinitionsForTextures ();

			// shows existing terrain definitions
			void showTerrainDefinitionForTexture ();

			const DataBlock* getTerrainDefitionForTexture (const std::string& texture);
	};
}
