#include "Rock.h"

#include "radiant_i18n.h"

#include "gtkutil/dialog.h"

namespace brushconstruct
{
	const std::string Rock::getName () const
	{
		return "brushRock";
	}

	void Rock::generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
			const std::string& shader)
	{
		if (sides < _minSides) {
			gtkutil::errorDialog(_("Too few sides for constructing the rock, minimum is 10"));
			return;
		}
		if (sides > _maxSides) {
			gtkutil::errorDialog(_("Too many sides for constructing the rock, maximum is 1000"));
			return;
		}

		brush.clear();
		brush.reserve(sides * sides);

		float radius = maxExtent(bounds.extents);
		const Vector3& mid = bounds.origin;
		Vector3 planepts[3];

		for (std::size_t j = 0; j < sides; j++) {
			planepts[0][0] = rand() - (RAND_MAX / 2);
			planepts[0][1] = rand() - (RAND_MAX / 2);
			planepts[0][2] = rand() - (RAND_MAX / 2);
			vector3_normalise(planepts[0]);

			// find two vectors that are perpendicular to planepts[0]
			ComputeAxisBase(planepts[0], planepts[1], planepts[2]);

			planepts[0] = mid + (planepts[0] * radius);
			planepts[1] = planepts[0] + (planepts[1] * radius);
			planepts[2] = planepts[0] + (planepts[2] * radius);

#if 0
			// make sure the orientation is right
			if (vector3_dot(vector3_subtracted(planepts[0], mid), vector3_cross(vector3_subtracted(planepts[1], mid), vector3_subtracted(planepts[2], mid))) > 0) {
				Vector3 h;
				h = planepts[1];
				planepts[1] = planepts[2];
				planepts[2] = h;
				globalOutputStream() << "flip\n";
			} else {
				globalOutputStream() << "no flip\n";
			}
#endif

			brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
		}
	}
}
