#ifndef TERRAIN_H_
#define TERRAIN_H_

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

			void showTerrainDefinitionForTexture ();
	};
}

#endif /* TERRAIN_H_ */
