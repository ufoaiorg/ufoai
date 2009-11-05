#ifndef MAPDEF_H_
#define MAPDEF_H_

#include "../common/Parser.h"

namespace scripts
{
	class MapDef
	{
		private:

			// the parser object that holds all the block data
			Parser parser;

			// the blocks with the data - destroyed with the parser
			std::vector<DataBlock*> _blocks;

		public:
			virtual ~MapDef ();

			MapDef ();

			/**
			 * Will try to add the current loaded map to the ufo script file with its own mapdef entry.
			 * In case the current loaded map is part of a ump the ump will be added.
			 * @return @c true if the map was added to the script, @c false if it wasn't possible or the
			 * map is already included in the script
			 */
			bool add ();

			void showMapDefinition ();
	};
}

#endif /* MAPDEF_H_ */
