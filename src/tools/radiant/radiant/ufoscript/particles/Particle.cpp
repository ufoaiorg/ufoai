#include "Particle.h"

namespace scripts
{
	Particle::Particle () :
		parser("particle")
	{
		_blocks = parser.getEntries();
	}

	Particle::~Particle ()
	{
	}

	void Particle::render ()
	{
	}

	std::string Particle::toString ()
	{
		return "";
	}

	scripts::IParticlePtr Particle::load (const std::string& particleID)
	{
		Particle *particle = new Particle();

		// TODO:

		scripts::IParticlePtr obj(particle);
		return obj;
	}
}
