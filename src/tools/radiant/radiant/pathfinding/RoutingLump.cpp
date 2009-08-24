#include "RoutingLump.h"

namespace routing
{
	RoutingLumpEntry::RoutingLumpEntry() : _origin(0., 0., 0.) {
	}

	const Vector3& RoutingLumpEntry::getOrigin() const {
		return _origin;
	}

	const EConnectionState RoutingLumpEntry::getConnectionState(const EDirection direction) const {
		return _connectionStates[direction];
	}


	RoutingLump::RoutingLump ()
	{
	}

	RoutingLump::~RoutingLump ()
	{
	}

	routing::RoutingLumpEntries& RoutingLump::getEntries()
	{
		return _entries;
	}

	void RoutingLump::add (const RoutingLumpEntry& dataEntry)
	{
		_entries.push_back(dataEntry);
	}
}
