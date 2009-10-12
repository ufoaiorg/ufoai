#ifndef TERRAIN_H_
#define TERRAIN_H_

#include "BrushConstructor.h"

namespace brushconstruct
{
	class Terrain: public BrushConstructor
	{
		private:
			static const std::size_t _sides = 6;

		public:
			void generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
					const std::string& shader);

			const std::string getName () const;

			static BrushConstructor& getInstance ()
			{
				static Terrain _rock;
				return _rock;
			}
	};
}
#endif /* TERRAIN_H_ */
