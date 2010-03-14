#include "Particle.h"
#include "../common/Parser.h"
#include "stream/memstream.h"
#include "script/scripttokeniser.h"

namespace scripts
{
	Particle::Particle ()
	{
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
		Parser parser("particle");
		DataBlock *data = parser.getEntryForID(particleID);
		if (data == (DataBlock*)0)
			return IParticlePtr();

		Particle *particle = new Particle();
		BufferInputStream stream(data->getData());
		ScriptTokeniser tokeniser(stream, false);

		for (;;) {
			std::string token = tokeniser.getToken();
			if (token.empty())
				break;

			// TODO:
		}

		scripts::IParticlePtr obj(particle);
		return obj;
	}
}
