#pragma once

#include "../../render/frontend/SphereRenderable.h"

class RenderLightRadiusWire: public SphereRenderable {
public:
	RenderLightRadiusWire (const Vector3& origin) :
			SphereRenderable(true, origin)
	{
	}
	void render (RenderStateFlags state) const
	{
		drawWire(_origin, _radius, 24);
		drawWire(_origin, _radius * 2.0f, 24);
	}
};
