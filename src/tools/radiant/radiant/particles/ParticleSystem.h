#include "iparticles.h"
#include "../ufoscript/common/Parser.h"

/**
 * Singleton class for the particle system
 */
class ParticleSystem: public IParticleSystem
{
	private:

		// the blocks with the data - destroyed with the parser
		typedef std::vector<scripts::DataBlock*> DataBlocks;
		DataBlocks _blocks;

		ParticleDefinitionMap _particleDefintions;
		bool _init;

	public:
		typedef IParticleSystem Type;
		STRING_CONSTANT(Name, "*");

		// constructs the particle system
		ParticleSystem();

		// frees the allocated heap memory
		~ParticleSystem();

		IParticleSystem* getTable() {
			return this;
		}

	private:

	public:
		void foreachParticle (const Visitor& visitor) const;

		const ParticleDefinitionMap& getParticleDefinitions ();

		IParticleDefinition* getParticle(const std::string& particleID);
};
