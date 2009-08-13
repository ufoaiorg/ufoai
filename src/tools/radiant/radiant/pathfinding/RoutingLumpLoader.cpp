#include "RoutingLumpLoader.h"
#include "autoptr.h"
#include "ifilesystem.h"

namespace routing
{
	void RoutingLumpLoader::loadRoutingLump (const ArchiveFile& file)
	{
		// TODO:
	}

	RoutingLumpLoader::RoutingLumpLoader (const std::string& bspFileName)
	{
		// Open an ArchiveFile to load
		AutoPtr<ArchiveFile> file(GlobalFileSystem().openFile(bspFileName.c_str()));
		if (file) {
			// Load the model and return the RenderablePtr
			loadRoutingLump(*file);
		}
	}

	routing::RoutingLump& RoutingLumpLoader::getRoutingLump ()
	{
		return _routingLump;
	}

	RoutingLumpLoader::~RoutingLumpLoader ()
	{
	}
}
