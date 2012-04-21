#ifndef UMPTILE_H_
#define UMPTILE_H_

#include <string>

namespace map
{
	namespace ump
	{
		/**
		 * A ump tile represents one bsp map that is part of a RMA (random map assembly). Such a tile must have
		 * some boundaries defined. For more information on what a ump tile is, see
		 * http://ufoai.org/wiki/index.php/Mapping/Random_map_assembly
		 */
		class UMPTile
		{
			private:

				// width of the tile + 2.
				// The units are not the world units here, but one unit in ump is about 128 world units
				// + 2 because you also define the boundaries of a tile
				int _width;

				// height of the tile + 2.
				// The units are not the world units here, but one unit in ump is about 128 world units
				// + 2 because you also define the boundaries of a tile
				int _height;

				std::string _mapName;

			private:

			public:

				UMPTile (std::string& mapName, int width, int height);

				virtual ~UMPTile ();

				/**
				 * @param base The base that should be subtracted from the map name to get the tile name
				 * @return The tile string
				 */
				std::string toString (const std::string& base);

				const std::string getTileName (const std::string& base) const;
		};
	}
}

#endif /* UMPTILE_H_ */
