#include "RoutingLump.h"

namespace routing
{
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
}
