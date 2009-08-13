#ifndef ROUTINGRENDERABLE_H_
#define ROUTINGRENDERABLE_H_

#include "irender.h"
#include <list>

namespace routing
{
	class RoutingRenderableEntry
	{
		private:
			Vector3 _origin;
		public:
			RoutingRenderableEntry (const Vector3& origin) :
				_origin(origin)
			{
			}
	};

	typedef std::list<RoutingRenderableEntry*> RoutingRenderableEntries;

	class RoutingRenderable: public OpenGLRenderable
	{
		private:
			RoutingRenderableEntries _entries;
		public:
			RoutingRenderable ();

			~RoutingRenderable ();

			/** Render function from OpenGLRenderable
			 */
			void render (RenderStateFlags flags) const;

			/** Adds a new renderable object to the list
			 */
			void add (const Vector3& origin);
	};
}

#endif /* ROUTINGRENDERABLE_H_ */
