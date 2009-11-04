#ifndef UMPFILE_H_
#define UMPFILE_H_

#include <string>
#include <vector>
#include "UMPTile.h"
#include "UMPAssembly.h"
#include "UMPException.h"
#include "iscriplib.h"

namespace map
{
	namespace ump
	{
		class UMPFile
		{
			private:
				// the filename of the ump - relative to maps/
				std::string _fileName;

				// the base of the ump tile to make the names shorter
				std::string _base;

				// list of all ump tiles
				std::vector<UMPTile> _umpTiles;
				typedef std::vector<UMPTile>::iterator UMPTileIterator;
				typedef std::vector<UMPTile>::const_iterator UMPTileConstIterator;

				// list of all ump assemblies
				std::vector<UMPAssembly> _umpAssenblies;
				typedef std::vector<UMPAssembly>::iterator UMPAssemblyIterator;
				typedef std::vector<UMPAssembly>::const_iterator UMPAssemblyConstIterator;

			private:

				/**
				 * @throw UMPException if tile could not get parsed
				 */
				void parseTile (Tokeniser &tokeniser) throw(UMPException);
				void parse (Tokeniser &tokenizer);

			public:

				/**
				 * Creates a new umpFile object
				 * @param fileName The ump file to create or read
				 * @param base The prefix for all ump tiles
				 */
				UMPFile (const std::string& fileName, const std::string& base = "");

				virtual ~UMPFile ();

				/**
				 * Load the ump file
				 * @return @c true if the load was successful, @c false otherwise
				 */
				bool load ();

				/**
				 * Save the ump file
				 * @return @c true if the save was successful, @c false otherwise
				 */
				bool save ();

				/**
				 * @return The filename of the ump file
				 */
				const std::string& getFileName () const;

				/**
				 * @return The prefix for all ump tiles
				 */
				const std::string& getBase () const;

				/**
				 * Adds a new ump tile to the file
				 * @param tile The tile to add
				 */
				void addTile (UMPTile& tile);

				/**
				 * @param map The name of the map to search the ump file for (base is handled automatically)
				 * @return @c NULL if not found, the tile pointer otherwise
				 */
				UMPTile* getTileForMap (const std::string& map);

				/**
				 * @param tile The tile to get the map name for
				 * @return The map name that was assembled by the tile name and the base (if a base is set)
				 */
				const std::string getMapName (const std::string& tile) const;

				/**
				 * Converts the map name into a tile name when a base is set
				 * @param map The map to get the tile name for
				 * @return The tile name that might be shorter if a base is set
				 */
				const std::string getTileName (const std::string& map) const;
		};
	}
}

#endif /* UMPFILE_H_ */
