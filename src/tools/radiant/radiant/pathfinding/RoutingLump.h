#ifndef ROUTINGLUMP_H_
#define ROUTINGLUMP_H_

#include <list>
#include <string>

namespace routing
{
	// TODO: Whatever there is in the lump
	typedef std::string RoutingLumpEntry;
	typedef std::list<RoutingLumpEntry> RoutingLumpEntries;

	class RoutingLump
	{
		private:
			RoutingLumpEntries _entries;

		public:
			RoutingLump ();

			virtual ~RoutingLump ();

			RoutingLumpEntries& getEntries();
	};
}
#endif /* ROUTINGLUMP_H_ */
