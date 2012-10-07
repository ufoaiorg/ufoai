#pragma once

#include "iparticles.h"

#include "render.h"

/**
 * @brief Render class for the grid windows
 */
class RenderableParticleID: public OpenGLRenderable {
	const IParticleDefinition* m_particle;
	const Vector3 &m_origin;
public:
	RenderableParticleID (const IParticleDefinition* particle, const Vector3 &origin) :
			m_particle(particle), m_origin(origin)
	{
	}

	void render (RenderStateFlags state) const
	{
		if (m_particle != 0) {
			glRasterPos3fv(m_origin);
			GlobalOpenGL().drawString(m_particle->getName());
		}
	}
};
