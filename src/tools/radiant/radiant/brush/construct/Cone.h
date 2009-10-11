#ifndef CONE_H_
#define CONE_H_

#include "BrushConstructor.h"

namespace brushconstruct
{
	class Cone: public BrushConstructor
	{
		private:
			static const std::size_t _minSides = 3;
			static const std::size_t _maxSides = 32;

		public:
			void generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
					const std::string& shader);

			const std::string getName() const;

			static BrushConstructor& getInstance ()
			{
				static Cone _cone;
				return _cone;
			}
	};
}
#endif /* CONE_H_ */
