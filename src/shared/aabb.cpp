#include "aabb.h"

AABB::AABB ()
{
	VectorCopy(vec3_origin, mins);
	VectorCopy(vec3_origin, maxs);
}
AABB::AABB (const vec3_t mini, const vec3_t maxi)
{
	VectorCopy(mini, mins);
	VectorCopy(maxi, maxs);
}
/**
 * @brief If the point is outside the box, expand the box to accommodate it.
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
/**
 * @brief If the given box is outside our box, expand our box to accommodate it.
 * @note  We only have to check min vs. min and max vs. max here. So adding a box is far more
 * efficient than adding it's min and max as points.
 */
void AABB::add (const AABB& other)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (other.mins[i] < mins[i])
			mins[i] = other.mins[i];
		if (other.maxs[i] > maxs[i])
			maxs[i] = other.maxs[i];
	}
}
