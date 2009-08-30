#ifndef ROUTINGLUMPLOADER_H_
#define ROUTINGLUMPLOADER_H_

#include "RoutingLump.h"
#include <string>
#include "iarchive.h"

namespace routing
{
	class RoutingLumpLoader
	{
		private:
			// the loaded routing data
			routing::RoutingLump _routingLump;

			void loadRoutingLump (ArchiveFile& file);

		public:
			RoutingLumpLoader ();

			// loads the routing lump for the given bsp file
			void loadRouting(const std::string& bspFileName);

			virtual ~RoutingLumpLoader ();

			// returns the loaded routing lump
			routing::RoutingLump& getRoutingLump ();
	};
}
#endif /* ROUTINGLUMPLOADER_H_ */
