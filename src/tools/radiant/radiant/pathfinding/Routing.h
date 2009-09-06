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
			bool _showPathfinding;
			bool _showIn2D;

		public:
			Routing ();

			virtual ~Routing ();

			/** Submit renderable geometry when rendering takes place in Solid mode. */
			void renderSolid (Renderer& renderer, const VolumeTest& volume) const;

			/** Submit renderable geometry when rendering takes place in Wireframe mode */
			void renderWireframe (Renderer& renderer, const VolumeTest& volume) const;

			void renderComponents (Renderer&, const VolumeTest&);

			void updateRouting (const std::string& bspFileName);

			void setShowAllLowerLevels (bool showAllLowerLevels)
			{
				_renderable.setShowAllLowerLevels(showAllLowerLevels);
			}

			void setShowIn2D (bool showIn2D)
			{
				_showIn2D = showIn2D;
			}

			void setShowPathfinding (bool showPathfinding)
			{
				_showPathfinding = showPathfinding;
			}
	};
}

#endif /* ROUTING_H_ */
