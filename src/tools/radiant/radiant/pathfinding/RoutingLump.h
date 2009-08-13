#ifndef ROUTINGLUMP_H_
#define ROUTINGLUMP_H_

#include <list>
#include "generic/vector.h"

namespace routing
{
	// TODO: Whatever there is in the lump
	class RoutingLumpEntry
	{
		private:
			Vector3 _origin;
		public:
			RoutingLumpEntry ();

			const Vector3& getOrigin () const;
	};

	typedef std::list<RoutingLumpEntry> RoutingLumpEntries;

	class RoutingLump
	{
		private:
			RoutingLumpEntries _entries;

		public:
			RoutingLump ();

			virtual ~RoutingLump ();

			RoutingLumpEntries& getEntries ();
	};
}
#endif /* ROUTINGLUMP_H_ */
