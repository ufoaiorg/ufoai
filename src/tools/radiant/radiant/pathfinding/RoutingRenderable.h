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
			void renderConnection (EDirection direction) const;
		public:
			RoutingRenderableEntry (const RoutingLumpEntry& data) :
				_data(data)
			{
			}

			void render (RenderStateFlags state) const;

			bool isForLevel (int level) const;
	};

	typedef std::list<RoutingRenderableEntry*> RoutingRenderableEntries;

	class RoutingRenderable: public OpenGLRenderable
	{
		private:
			RoutingRenderableEntries _entries;
			mutable int _glListID;
			bool _showAllLowerLevels;
			void checkClearGLCache (void);

		public:
			RoutingRenderable ();

			~RoutingRenderable ();

			/** Render function from OpenGLRenderable
			 */
			void render (RenderStateFlags flags) const;

			void setShowAllLowerLevels (bool showAllLowerLevels);

			/** Creates a new renderable object for given data lump and adds it to the list
			 */
			void add (const RoutingLumpEntry& data);
			/** Clear list of renderables after updating rendering data */
			void clear ();
	};
}

#endif /* ROUTINGRENDERABLE_H_ */
