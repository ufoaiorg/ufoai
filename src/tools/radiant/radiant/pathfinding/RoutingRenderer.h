#ifndef ROUTINGRENDERER_H_
#define ROUTINGRENDERER_H_

#include "irender.h"

namespace routing
{
	class RoutingRenderer: public OpenGLRenderable
	{
		private:
			Vector3 m_origin;

		public:
			RoutingRenderer (Vector3& origin);

			~RoutingRenderer ();

			/** Render function from OpenGLRenderable
			 */
			void render (RenderStateFlags flags) const;
	};
}

#endif /* ROUTINGRENDERER_H_ */
