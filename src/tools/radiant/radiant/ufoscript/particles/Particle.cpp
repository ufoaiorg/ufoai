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
}
