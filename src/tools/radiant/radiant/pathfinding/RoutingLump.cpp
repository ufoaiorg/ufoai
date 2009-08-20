#include "RoutingLump.h"

namespace routing
{
	RoutingLumpEntry::RoutingLumpEntry() : _origin(0., 0., 0.) {
	}

	const Vector3& RoutingLumpEntry::getOrigin() const {
		return _origin;
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
