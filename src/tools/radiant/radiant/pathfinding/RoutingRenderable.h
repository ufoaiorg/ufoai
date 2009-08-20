#ifndef ROUTINGRENDERABLE_H_
#define ROUTINGRENDERABLE_H_

#include "irender.h"
#include <list>
#include "RoutingLump.h"

namespace routing
{
	class RoutingRenderableEntry
	{
		private:
			RoutingLumpEntry _data;
			void renderWireframe (void) const;
			void renderAccessability (RenderStateFlags state) const;
			void renderConnections (void) const;
			void renderConnection (int direction) const;
		public:
			RoutingRenderableEntry (const RoutingLumpEntry& data) :
				_data(data)
			{
			}

			void render (RenderStateFlags state) const;
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

			/** Creates a new renderable object for given data lump and adds it to the list
			 */
			void add (const RoutingLumpEntry& data);
	};
}

#endif /* ROUTINGRENDERABLE_H_ */
