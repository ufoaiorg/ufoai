#include "Sphere.h"

#include "radiant_i18n.h"

#include "gtkutil/dialog.h"

namespace brushconstruct
{
	const std::string Sphere::getName () const
	{
		return "brushSphere";
	}

	void Sphere::generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
			const std::string& shader)
	{
		if (sides < _minSides) {
			gtkutil::errorDialog(_("Too few sides for constructing the sphere, minimum is 3"));
			return;
		}
		if (sides > _maxSides) {
			gtkutil::errorDialog(_("Too many sides for constructing the sphere, maximum is 31"));
			return;
		}

		brush.clear();
		brush.reserve(sides * sides);

		float radius = maxExtent(bounds.extents);
		const Vector3& mid = bounds.origin;
		Vector3 planepts[3];

		double dt = 2 * c_pi / sides;
		double dp = c_pi / sides;
		for (std::size_t i = 0; i < sides; i++) {
			for (std::size_t j = 0; j < sides - 1; j++) {
				double t = i * dt;
				double p = float(j * dp - c_pi / 2);

				planepts[0] = mid + vector3_for_spherical(t, p) * radius;
				planepts[1] = mid + vector3_for_spherical(t, p + dp) * radius;
				planepts[2] = mid + vector3_for_spherical(t + dt, p + dp) * radius;

				brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
			}
		}

		{
			double p = (sides - 1) * dp - c_pi / 2;
			for (std::size_t i = 0; i < sides; i++) {
				double t = i * dt;

				planepts[0] = mid + vector3_for_spherical(t, p) * radius;
				planepts[1] = mid + vector3_for_spherical(t + dt, p + dp) * radius;
				planepts[2] = mid + vector3_for_spherical(t + dt, p) * radius;

				brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
			}
		}
	}
}
