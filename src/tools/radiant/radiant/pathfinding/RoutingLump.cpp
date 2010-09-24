#include "RoutingLump.h"

namespace routing
{
	RoutingLumpEntry::RoutingLumpEntry(Vector3 origin, int level) : _origin(origin), _level(level) {
	}

	RoutingLumpEntry::RoutingLumpEntry (const RoutingLumpEntry &other) :
		_origin(other._origin), _level(other._level), _accessState(other._accessState)
	{
		/** @todo easier way to copy array? */
		for (EDirection dir = DIR_WEST; dir < MAX_DIRECTIONS; dir++ ) {
			_connectionStates[dir] = other._connectionStates[dir];
		}
	}

	EConnectionState RoutingLumpEntry::getConnectionState(const EDirection direction) const {
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
