#ifndef CUBOID_H_
#define CUBOID_H_

#include "BrushConstructor.h"

namespace brushconstruct
{
	class Cuboid: public BrushConstructor
	{
		public:
			void generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
					const std::string& shader);

			const std::string getName() const;

			static BrushConstructor& getInstance ()
			{
				static Cuboid _cuboid;
				return _cuboid;
			}
	};
}
#endif /* CUBOID_H_ */
