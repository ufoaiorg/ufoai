#pragma once

#include "iparticles.h"
#include "irender.h"

/**
 * @brief Render class for the 3d view
 */
class RenderableParticle: public OpenGLRenderable {
	const IParticleDefinition* m_particle;
public:
	RenderableParticle (const IParticleDefinition* particle) :
			m_particle(particle)
	{
	}

	void render (RenderStateFlags state) const
	{
		/** @todo render the image and/or model */
	}
};
