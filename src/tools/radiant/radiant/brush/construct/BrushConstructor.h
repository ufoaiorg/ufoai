#ifndef BRUSHCONSTRUCTOR_H_
#define BRUSHCONSTRUCTOR_H_

#include <string>
#include "../Brush.h"

namespace brushconstruct
{
	class BrushConstructor
	{
		protected:
			float maxExtent (const Vector3& extents)
			{
				return std::max(std::max(extents[0], extents[1]), extents[2]);
			}

		public:
			virtual ~BrushConstructor() {}

			/**
			 * @param[out] brush The brush to create the planes for
			 * @param[in] bounds The mins and maxs of the cube
			 * @param[in] sides
			 * @param[in] projection The texture projection that is used (shift, scale and rotate values)
			 * @param[in] shader The path of the texture relative to the base dir
			 */
			virtual void generate (Brush& brush, const AABB& bounds, std::size_t sides,
					const TextureProjection& projection, const std::string& shader) = 0;

			virtual const std::string getName () const = 0;
	};
}
#endif /* BRUSHCONSTRUCTOR_H_ */
