#include "Prism.h"

#include "radiant_i18n.h"

#include "gtkutil/dialog.h"

namespace brushconstruct
{
	const std::string Prism::getName () const
	{
		return "brushPrism";
	}

	void Prism::generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
			const std::string& shader)
	{
		if (sides < _minSides) {
			gtkutil::errorDialog(_("Too few sides for constructing the prism, minimum is 3"));
			return;
		}
		if (sides > _maxSides) {
			gtkutil::errorDialog(_("Too many sides for constructing the prism"));
			return;
		}

		brush.clear();
		brush.reserve(sides + 2);

		Vector3 mins(bounds.origin - bounds.extents);
		Vector3 maxs(bounds.origin + bounds.extents);

		int axis = getViewAxis();
		float radius = getMaxExtent2D(bounds.extents, axis);
		const Vector3& mid = bounds.origin;
		Vector3 planepts[3];

		planepts[2][(axis + 1) % 3] = mins[(axis + 1) % 3];
		planepts[2][(axis + 2) % 3] = mins[(axis + 2) % 3];
		planepts[2][axis] = maxs[axis];
		planepts[1][(axis + 1) % 3] = maxs[(axis + 1) % 3];
		planepts[1][(axis + 2) % 3] = mins[(axis + 2) % 3];
		planepts[1][axis] = maxs[axis];
		planepts[0][(axis + 1) % 3] = maxs[(axis + 1) % 3];
		planepts[0][(axis + 2) % 3] = maxs[(axis + 2) % 3];
		planepts[0][axis] = maxs[axis];

		brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);

		planepts[0][(axis + 1) % 3] = mins[(axis + 1) % 3];
		planepts[0][(axis + 2) % 3] = mins[(axis + 2) % 3];
		planepts[0][axis] = mins[axis];
		planepts[1][(axis + 1) % 3] = maxs[(axis + 1) % 3];
		planepts[1][(axis + 2) % 3] = mins[(axis + 2) % 3];
		planepts[1][axis] = mins[axis];
		planepts[2][(axis + 1) % 3] = maxs[(axis + 1) % 3];
		planepts[2][(axis + 2) % 3] = maxs[(axis + 2) % 3];
		planepts[2][axis] = mins[axis];

		brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);

		for (std::size_t i = 0; i < sides; ++i) {
			const double v  = i * (M_PI * 2 / sides);
			const double sv = sin(v);
			const double cv = cos(v);

			planepts[0][(axis + 1) % 3] = static_cast<float> (floor(mid[(axis + 1) % 3] + radius * cv + 0.5));
			planepts[0][(axis + 2) % 3] = static_cast<float> (floor(mid[(axis + 2) % 3] + radius * sv + 0.5));
			planepts[0][axis] = mins[axis];

			planepts[1][(axis + 1) % 3] = planepts[0][(axis + 1) % 3];
			planepts[1][(axis + 2) % 3] = planepts[0][(axis + 2) % 3];
			planepts[1][axis] = maxs[axis];

			planepts[2][(axis + 1) % 3] = static_cast<float> (floor(planepts[0][(axis + 1) % 3] - radius * sv + 0.5));
			planepts[2][(axis + 2) % 3] = static_cast<float> (floor(planepts[0][(axis + 2) % 3] + radius * cv + 0.5));
			planepts[2][axis] = maxs[axis];

			brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
		}
	}
}
