#ifndef SPHERE_H_
#define SPHERE_H_

#include "BrushConstructor.h"

namespace brushconstruct
{
	class Sphere: public BrushConstructor
	{
		private:
			static const std::size_t _minSides = 3;
			static const std::size_t _maxSides = 31;

		public:
			void generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
					const std::string& shader);

			const std::string getName () const;

			static BrushConstructor& getInstance ()
			{
				static Sphere _sphere;
				return _sphere;
			}
	};
}
#endif /* SPHERE_H_ */
