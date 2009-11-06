#ifndef PARTICLE_H_
#define PARTICLE_H_

#include "../common/Parser.h"

namespace scripts
{
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
	};
}

#endif /* PARTICLE_H_ */
