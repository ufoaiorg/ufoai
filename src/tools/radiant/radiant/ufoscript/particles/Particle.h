#ifndef PARTICLE_H_
#define PARTICLE_H_

#include "../common/Parser.h"
#include <memory>
#include <string>

namespace scripts
{
	class Particle;
	// Smart pointer typedef
	typedef std::auto_ptr<scripts::Particle> IParticlePtr;

	class Particle
	{
		private:

			// the parser object that holds all the block data
			Parser parser;

			// the blocks with the data - destroyed with the parser
			std::vector<DataBlock*> _blocks;

		public:
			Particle ();

			virtual ~Particle ();

			void render ();

			std::string toString ();

			static scripts::IParticlePtr load (const std::string& particleID);
	};
}

#endif /* PARTICLE_H_ */
