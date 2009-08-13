#include "Routing.h"
#include "math/matrix.h"
#include "autoptr.h"
#include <string>

namespace routing
{
	Routing::Routing (const std::string& bspFileName) : _loader(bspFileName)
	{
	}

	Routing::~Routing ()
	{
	}

	void renderRoutingLumpEntryToRenderer(RoutingLumpEntry& entry) {
		// TODO:
		//_renderable.add(entry.get....);
	}

	template<typename Functor>
	void visit (routing::RoutingLump& lump, const Functor& functor)
	{
		routing::RoutingLumpEntries& entries = lump.getEntries();
		for (routing::RoutingLumpEntries::iterator i = entries.begin(); i != entries.end(); ++i) {
			functor(*i);
		}
	}

	/** Submit renderable geometry when rendering takes place in Solid mode. */
	void Routing::renderSolid (Renderer& renderer, const VolumeTest& volume)
	{
		routing::RoutingLump& lump = _loader.getRoutingLump();
		visit(lump, renderRoutingLumpEntryToRenderer);
		renderer.addRenderable(_renderable, g_matrix4_identity);
	}

	/** Submit renderable geometry when rendering takes place in Wireframe mode */
	void Routing::renderWireframe (Renderer& renderer, const VolumeTest& volume)
	{
		// this is not needed for rendering the routing data
	}

	void Routing::renderComponents (Renderer&, const VolumeTest&)
	{
		// this is not needed for rendering the routing data
	}
}
