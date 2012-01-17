#include "ParticleSystem.h"
#include "iradiant.h"
#include "ifilesystem.h"
#include "irender.h"
#include "igl.h"
#include "iufoscript.h"
#include "ParticleDefinition.h"
#include "ParticleParser.h"

#include <vector>

ParticleSystem::ParticleSystem ()
{
}

ParticleSystem::~ParticleSystem ()
{
	for (ParticleDefinitionMap::iterator i = _particleDefintions.begin(); i != _particleDefintions.end(); ++i) {
		delete i->second;
	}
}

IParticleDefinition* ParticleSystem::getParticle (const std::string& particleID)
{
	if (particleID.empty())
		return NULL;

	const ParticleDefinitionMap &p = getParticleDefinitions();
	ParticleDefinitionMap::const_iterator i = p.find(particleID);
	if (i != p.end())
		return i->second;

	return NULL;
}

void ParticleSystem::foreachParticle (const Visitor& visitor) const
{
	for (ParticleDefinitionMap::const_iterator i = _particleDefintions.begin(); i != _particleDefintions.end(); ++i) {
		visitor.visit(i->second);
	}
}

const ParticleDefinitionMap& ParticleSystem::getParticleDefinitions ()
{
	if (!_init) {
		_init = true;
		scripts::Parser parser("particle");
		_blocks = parser.getEntries();
		for (DataBlocks::iterator i = _blocks.begin(); i != _blocks.end(); ++i) {
			const std::string& particleID = (*i)->getID();
			_particleDefintions[particleID] = ParticleParser(particleID, (*i)->getData()).getParticle();
		}
	}
	return _particleDefintions;
}

// Module stuff

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class ParticleSystemDependencies:
		public GlobalRadiantModuleRef,
		public GlobalFileSystemModuleRef,
		public GlobalShaderCacheModuleRef,
		public GlobalOpenGLModuleRef
{
};

typedef SingletonModule<ParticleSystem, ParticleSystemDependencies> ParticleSystemModule;
typedef Static<ParticleSystemModule> StaticParticleSystemModule;
StaticRegisterModule staticRegisterParticleSystem(StaticParticleSystemModule::instance());
