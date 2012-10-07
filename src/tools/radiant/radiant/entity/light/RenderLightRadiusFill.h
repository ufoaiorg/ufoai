#pragma once

#include "../../render/frontend/SphereRenderable.h"

class RenderLightRadiusFill: public SphereRenderable {
public:
	RenderLightRadiusFill (const Vector3& origin) :
			SphereRenderable(false, origin)
	{
	}

	void render (RenderStateFlags state) const
	{
		drawFill(_origin, _radius, 16);
		drawFill(_origin, _radius * 2.0f, 16);
	}
};
