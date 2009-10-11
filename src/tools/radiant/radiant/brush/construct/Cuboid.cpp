#include "Cuboid.h"

namespace brushconstruct
{
	const std::string Cuboid::getName () const
	{
		return "brushCuboid";
	}

	/**
	 * @brief Creates a cube out of the given mins and maxs of the axis aligned bounding box
	 * @param[out] The brush to creates the planes for
	 * @param[in] bounds The mins and maxs of the cube
	 * @param[in] shader The path of the texture relative to the base dir
	 * @param[in] projection The texture projection that is used (shift, scale and rotate values)
	 */
	void Cuboid::generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
			const std::string& shader)
	{
		const unsigned char box[3][2] = { { 0, 1 }, { 2, 0 }, { 1, 2 } };
		Vector3 mins(bounds.origin - bounds.extents);
		Vector3 maxs(bounds.origin + bounds.extents);

		brush.clear();
		brush.reserve(6);

		{
			for (int i = 0; i < 3; ++i) {
				Vector3 planepts1(maxs);
				Vector3 planepts2(maxs);
				planepts2[box[i][0]] = mins[box[i][0]];
				planepts1[box[i][1]] = mins[box[i][1]];

				brush.addPlane(maxs, planepts1, planepts2, shader, projection);
			}
		}
		{
			for (int i = 0; i < 3; ++i) {
				Vector3 planepts1(mins);
				Vector3 planepts2(mins);
				planepts1[box[i][0]] = maxs[box[i][0]];
				planepts2[box[i][1]] = maxs[box[i][1]];

				brush.addPlane(mins, planepts1, planepts2, shader, projection);
			}
		}
	}
}
