#include "RoutingRenderable.h"

namespace routing
{
	RoutingRenderable::RoutingRenderable ()
	{
	}

	RoutingRenderable::~RoutingRenderable ()
	{
		for (routing::RoutingRenderableEntries::const_iterator i = _entries.begin(); i != _entries.end(); ++i) {
			delete *i;
		}
	}

	void RoutingRenderable::render (RenderStateFlags state) const
	{
		for (routing::RoutingRenderableEntries::const_iterator i = _entries.begin(); i != _entries.end(); ++i) {
			const routing::RoutingRenderableEntry* entry = *i;
			// TODO: Render it
			//entry->_origin;
		}
	}

	//TODO: origin isn't enough
	void RoutingRenderable::add (const Vector3& origin)
	{
		routing::RoutingRenderableEntry* entry = new RoutingRenderableEntry(origin);
		_entries.push_back(entry);
	}
}
