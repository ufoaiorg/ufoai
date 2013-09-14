#pragma once

#include "iparticles.h"
#include "irender.h"

/**
 * @brief Render class for the 3d view
 */
class RenderableParticle: public OpenGLRenderable {
public:
	RenderableParticle (const IParticleDefinition* particle)
	{
	}

	void render (RenderStateFlags state) const
	{
		/** @todo render the image and/or model */
	}
};
