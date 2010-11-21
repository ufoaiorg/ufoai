#include "Cone.h"

#include "radiant_i18n.h"

#include "gtkutil/dialog.h"

namespace brushconstruct
{
	const std::string Cone::getName () const
	{
		return "brushCone";
	}

	void Cone::generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
			const std::string& shader)
	{
		if (sides < _minSides) {
			gtkutil::errorDialog(_("Too few sides for constructing the prism, minimum is 3"));
			return;
		}
		if (sides > _maxSides) {
			gtkutil::errorDialog(_("Too many sides for constructing the prism, maximum is 32"));
			return;
		}

		brush.clear();
		brush.reserve(sides + 1);

		Vector3 mins(bounds.origin - bounds.extents);
		Vector3 maxs(bounds.origin + bounds.extents);

		float radius = maxExtent(bounds.extents);
		const Vector3& mid = bounds.origin;
		Vector3 planepts[3];

		planepts[0][0] = mins[0];
		planepts[0][1] = mins[1];
		planepts[0][2] = mins[2];
		planepts[1][0] = maxs[0];
		planepts[1][1] = mins[1];
		planepts[1][2] = mins[2];
		planepts[2][0] = maxs[0];
		planepts[2][1] = maxs[1];
		planepts[2][2] = mins[2];

		brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);

		for (std::size_t i = 0; i < sides; ++i) {
			double sv = sin(i * 3.14159265 * 2 / sides);
			double cv = cos(i * 3.14159265 * 2 / sides);

			planepts[0][0] = static_cast<float> (floor(mid[0] + radius * cv + 0.5));
			planepts[0][1] = static_cast<float> (floor(mid[1] + radius * sv + 0.5));
			planepts[0][2] = mins[2];

			planepts[1][0] = mid[0];
			planepts[1][1] = mid[1];
			planepts[1][2] = maxs[2];

			planepts[2][0] = static_cast<float> (floor(planepts[0][0] - radius * sv + 0.5));
			planepts[2][1] = static_cast<float> (floor(planepts[0][1] + radius * cv + 0.5));
			planepts[2][2] = maxs[2];

			brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
		}
	}
}
