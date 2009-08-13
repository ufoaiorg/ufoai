#ifndef ROUTING_H_
#define ROUTING_H_

#include "cullable.h"
#include "renderable.h"
#include "RoutingLumpLoader.h"
#include "RoutingRenderable.h"

namespace routing
{
	class Routing: public Renderable
	{
		private:
			// responsible for rendering the routing data
			RoutingRenderable _renderable;
			RoutingLumpLoader _loader;

		public:
			Routing (const std::string& bspFileName);

			virtual ~Routing ();

			/** Submit renderable geometry when rendering takes place in Solid mode. */
			void renderSolid (Renderer& renderer, const VolumeTest& volume);

			/** Submit renderable geometry when rendering takes place in Wireframe mode */
			void renderWireframe (Renderer& renderer, const VolumeTest& volume);

			void renderComponents (Renderer&, const VolumeTest&);
	};
}

#endif /* ROUTING_H_ */
