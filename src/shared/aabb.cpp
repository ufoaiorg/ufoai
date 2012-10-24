#include "aabb.h"

AABB::AABB (const vec3_t mini, const vec3_t maxi)
{
	VectorCopy(mini, mins);
	VectorCopy(maxi, maxs);
}
/**
 * @brief If the point is outside the box defined by mins and maxs, expand
 * the box to accommodate it.
 */
void AABB::add (const vec3_t point)
{
	int i;
	vec_t val;

	for (i = 0; i < 3; i++) {
		val = point[i];
		if (val < mins[i])
			mins[i] = val;
		if (val > maxs[i])
			maxs[i] = val;
	}
}
