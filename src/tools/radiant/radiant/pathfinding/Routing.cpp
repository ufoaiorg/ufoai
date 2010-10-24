#include "Routing.h"
#include "math/matrix.h"
#include "AutoPtr.h"
#include <string>
#include "eclasslib.h"

namespace routing
{
	/** @todo release this shader */
	Shader *m_routingShader;

	const Colour3 color2 = Colour3(1, 1, 1);

	Routing::Routing () :
		_showPathfinding(false), _showIn2D(false)
	{
		m_routingShader = 0;
	}

	Routing::~Routing ()
	{
		if (m_routingShader)
			colour_release_state_fill(color2);
		m_routingShader = 0;
	}

	/** Submit renderable geometry when rendering takes place in Solid mode. */
	void Routing::renderSolid (Renderer& renderer, const VolumeTest& volume) const
	{
		if (_showPathfinding) {
			/** @todo move this shader init somewhere else? */
			if (!m_routingShader)
				m_routingShader = colour_capture_state_fill(color2);
			// renderer must have shader set for adding renderable
			renderer.SetState(m_routingShader, Renderer::eFullMaterials);
			renderer.addRenderable(_renderable, Matrix4::getIdentity());
		}
	}

	/** Submit renderable geometry when rendering takes place in Wireframe mode */
	void Routing::renderWireframe (Renderer& renderer, const VolumeTest& volume) const
	{
		/**@todo check whether to create a second renderable for wire mode */
		if (_showPathfinding && _showIn2D) {
			/** @todo move this shader init somewhere else? */
			if (!m_routingShader)
				m_routingShader = colour_capture_state_fill(color2);
			// renderer must have shader set for adding renderable
			renderer.SetState(m_routingShader, Renderer::eWireframeOnly);
			renderer.addRenderable(_renderable, Matrix4::getIdentity());
		}
	}

	void Routing::renderComponents (Renderer&, const VolumeTest&)
	{
		// this is not needed for rendering the routing data
	}

	/**
	 * @param bspFileName Relative bsp file name
	 */
	void Routing::updateRouting (const std::string& bspFileName)
	{
		_loader.loadRouting(bspFileName);
		routing::RoutingLump& lump = _loader.getRoutingLump();
		routing::RoutingLumpEntries& entries = lump.getEntries();
		_renderable.clear();
		for (routing::RoutingLumpEntries::const_iterator i = entries.begin(); i != entries.end(); ++i) {
			_renderable.add(*i);
		}
	}
}
