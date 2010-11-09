#include "iparticles.h"
#include <string>

class ParticleParser {
	private:
		const std::string& _particleData;
		const std::string& _particleID;

	public:
		// the particle data is the string from the ufo scripts
		ParticleParser(const std::string& particleID, const std::string& particleData);

		// return the parsed particle - allocated on the heap (so make sure you delete
		// it after you are done with it)
		IParticleDefinition* getParticle();
};
