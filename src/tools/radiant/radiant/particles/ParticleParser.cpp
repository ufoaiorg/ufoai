#include "ParticleParser.h"
#include "ParticleDefinition.h"

ParticleParser::ParticleParser (const std::string& particleID, const std::string& particleData) :
	_particleData(particleData), _particleID(particleID)
{
}

IParticleDefinition* ParticleParser::getParticle ()
{
	return new ParticleDefinition(_particleID, "", "", 0, 0);
}
