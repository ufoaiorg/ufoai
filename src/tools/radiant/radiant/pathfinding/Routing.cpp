#include "Routing.h"
#include "math/matrix.h"
#include "autoptr.h"
#include <string>
#include "eclasslib.h"

namespace routing
{

	Routing::Routing () :_showPathfinding(false)
	{
	}

	Routing::~Routing ()
	{
	}

	/** Submit renderable geometry when rendering takes place in Solid mode. */
	void Routing::renderSolid (Renderer& renderer, const VolumeTest& volume) const
	{
		if (_showPathfinding) {
			/**@todo move this shader init somewhere else? */
			const Colour3 color2 = Colour3(1, 1, 1);
			m_routingShader = colour_capture_state_fill(color2);
			//renderer must have shader set for adding renderable
			renderer.SetState(m_routingShader,Renderer::eFullMaterials);
			renderer.addRenderable(_renderable, g_matrix4_identity);
		}
	}

	/** Submit renderable geometry when rendering takes place in Wireframe mode */
	void Routing::renderWireframe (Renderer& renderer, const VolumeTest& volume) const
	{
		// this is not needed for rendering the routing data
	}

	void Routing::renderComponents (Renderer&, const VolumeTest&)
	{
		// this is not needed for rendering the routing data
	}

	void Routing::updateRouting(const std::string& bspFileName)
	{
		_loader.loadRouting(bspFileName);
		routing::RoutingLump& lump = _loader.getRoutingLump();
		routing::RoutingLumpEntries& entries = lump.getEntries();
		for (routing::RoutingLumpEntries::const_iterator i = entries.begin(); i != entries.end(); ++i) {
			_renderable.add(*i);
		}
	}
}
