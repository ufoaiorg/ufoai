/**
 * @file mathlib.c
 * @brief math primitives
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../common/common.h"

#ifndef logf
#define logf(x) ((float)log((double)(x)))
#endif

const vec2_t vec2_origin = { 0, 0 };
const vec3_t vec3_origin = { 0, 0, 0 };
const vec4_t vec4_origin = { 0, 0, 0, 0 };

/** @brief cos 45 degree */
#define RT2 0.70710678118654752440084436210485

/**
 * @note DIRECTIONS
 *  straight
 * 0 = x+1, y
 * 1 = x-1, y
 * 2 = x  , y+1
 * 3 = x  , y-1
 *  diagonal
 * 4 = x+1, y+1
 * 5 = x-1, y-1
 * 6 = x-1, y+1
 * 7 = x+1, y-1
 * @note (change in x, change in y, change in z, change in height status)
 */
const vec4_t dvecs[PATHFINDING_DIRECTIONS] = {
	{ 1,  0,  0,  0},	/* E */
	{-1,  0,  0,  0},	/* W */
	{ 0,  1,  0,  0},	/* N */
	{ 0, -1,  0,  0},	/* S */
	{ 1,  1,  0,  0},	/* NE */
	{-1, -1,  0,  0},	/* SW */
	{-1,  1,  0,  0},	/* NW */
	{ 1, -1,  0,  0},	/* SE */
	{ 0,  0,  1,  0},	/* CLIMB UP */
	{ 0,  0, -1,  0},	/* CLIMB DOWN */
	{ 0,  0,  0, -1},	/* STAND UP */
	{ 0,  0,  0,  1},	/* STAND DOWN */
	{ 0,  0,  0,  0},	/* UNDEFINED OPPOSITE OF FALL DOWN */
	{ 0,  0, -1,  0},	/* FALL DOWN */
	{ 0,  0,  0,  0},	/* UNDEFINED */
	{ 0,  0,  0,  0},	/* UNDEFINED */
	{ 1,  0,  1,  0},	/* UP E (Fliers only)*/
	{-1,  0,  1,  0},	/* UP W (Fliers only) */
	{ 0,  1,  1,  0},	/* UP N (Fliers only) */
	{ 0, -1,  1,  0},	/* UP S (Fliers only) */
	{ 1,  1,  1,  0},	/* UP NE (Fliers only) */
	{-1, -1,  1,  0},	/* UP SW (Fliers only) */
	{-1,  1,  1,  0},	/* UP NW (Fliers only) */
	{ 1, -1,  1,  0},	/* UP SE (Fliers only) */
	{ 1,  0,  0,  0},	/* E (Fliers only)*/
	{-1,  0,  0,  0},	/* W (Fliers only) */
	{ 0,  1,  0,  0},	/* N (Fliers only) */
	{ 0, -1,  0,  0},	/* S (Fliers only) */
	{ 1,  1,  0,  0},	/* NE (Fliers only) */
	{-1, -1,  0,  0},	/* SW (Fliers only) */
	{-1,  1,  0,  0},	/* NW (Fliers only) */
	{ 1, -1,  0,  0},	/* SE (Fliers only) */
	{ 1,  0, -1,  0},	/* DOWN E (Fliers only) */
	{-1,  0, -1,  0},	/* DOWN W (Fliers only) */
	{ 0,  1, -1,  0},	/* DOWN N (Fliers only) */
	{ 0, -1, -1,  0},	/* DOWN S (Fliers only) */
	{ 1,  1, -1,  0},	/* DOWN NE (Fliers only) */
	{-1, -1, -1,  0},	/* DOWN SW (Fliers only) */
	{-1,  1, -1,  0},	/* DOWN NW (Fliers only) */
	{ 1, -1, -1,  0},	/* DOWN SE (Fliers only) */
	};

/*                                           0:E     1:W      2:N     3:S      4:NE        5:SW          6:NW         7:SE  */
const float dvecsn[CORE_DIRECTIONS][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {RT2, RT2}, {-RT2, -RT2}, {-RT2, RT2}, {RT2, -RT2} };
/** @note if you change directionAngles[PATHFINDING_DIRECTIONS], you must also change function AngleToDV */
/*                                     0:E 1: W    2:N    3:S     4:NE   5:SW    6:NW    7:SE  */
const float directionAngles[CORE_DIRECTIONS] = { 0, 180.0f, 90.0f, 270.0f, 45.0f, 225.0f, 135.0f, 315.0f };

const byte dvright[CORE_DIRECTIONS] = { 7, 6, 4, 5, 0, 1, 2, 3 };
const byte dvleft[CORE_DIRECTIONS] = { 4, 5, 6, 7, 2, 3, 1, 0 };


/**
 * @brief Returns the indice of array directionAngles[DIRECTIONS] whose value is the closest to angle
 * @note This function allows to know the closest multiple of 45 degree of angle.
 * @param[in] angle The angle (in degrees) which is tested.
 * @return Corresponding indice of array directionAngles[DIRECTIONS].
 */
int AngleToDir (int angle)
{
	static const int anglesToDV[8] = {0, 4, 2, 6, 1, 5, 3, 7};

	angle += 22;
	/* set angle between 0 <= angle < 360 */
	angle %= 360;
	/* next step is because the result of angle %= 360 when angle is negative depends of the compiler
	 * (it can be between -360 < angle <= 0 or 0 <= angle < 360) */
	if (angle < 0)
		angle += 360;

	/* get an integer quotient */
	angle /= 45;

	if (angle >= 0 && angle < CORE_DIRECTIONS)
		return anglesToDV[angle];

	/* This is the default for unknown values. */
	Com_Printf("Error in AngleToDV: shouldn't have reached this line\n");
	return 0;
}

/**
 * @brief Round to nearest integer
 */
vec_t Q_rint (const vec_t in)
{
	/* round x down to the nearest integer */
	return floor(in + 0.5);
}


/**
 * @brief Calculate distance on the geoscape.
 * @param[in] pos1 Position at the start.
 * @param[in] pos2 Position at the end.
 * @return Distance from pos1 to pos2.
 * @note distance is an angle! This is the angle (in degrees) between the 2 vectors
 * starting at earth's center and ending at pos1 or pos2. (if you prefer distance,
 * this is also the distance on a globe of radius 180 / pi = 57)
 */
double GetDistanceOnGlobe (const vec2_t pos1, const vec2_t pos2)
{
	/* convert into rad */
	const double latitude1 = pos1[1] * torad;
	const double latitude2 = pos2[1] * torad;
	const double deltaLongitude = (pos1[0] - pos2[0]) * torad;
	double distance;

	distance = cos(latitude1) * cos(latitude2) * cos(deltaLongitude) + sin(latitude1) * sin(latitude2);
	distance = min(max(-1, distance), 1);
	distance = acos(distance) * todeg;

	assert(distance >= 0.0);
	return distance;
}

/**
 * @brief
 */
vec_t ColorNormalize (const vec3_t in, vec3_t out)
{
	float max;

	/* find the brightest component */
	max = in[0];
	if (in[1] > max)
		max = in[1];
	if (in[2] > max)
		max = in[2];

	/* avoid FPE */
	if (equal(max, 0.0)) {
		VectorClear(out);
		return 0;
	}

	VectorScale(in, 1.0 / max, out);

	return max;
}

/**
 * @brief Checks whether the given vector @c v1 is closer to @c comp as the vector @c v2
 * @param[in] v1 Vector to check whether it's closer to @c comp as @c v2
 * @param[in] v2 Vector to check against
 * @param[in] comp The vector to check the distance from
 * @return Returns true if @c v1 is closer to @c comp as @c v2
 */
qboolean VectorNearer (const vec3_t v1, const vec3_t v2, const vec3_t comp)
{
	vec3_t d1, d2;

	VectorSubtract(comp, v1, d1);
	VectorSubtract(comp, v2, d2);

	return VectorLength(d1) < VectorLength(d2);
}

/**
 * @brief Calculated the normal vector for a given vec3_t
 * @param[in] v Vector to normalize
 * @param[out] out The normalized vector
 * @sa VectorNormalize
 * @return vector length as vec_t
 * @sa CrossProduct
 */
vec_t VectorNormalize2 (const vec3_t v, vec3_t out)
{
	float length;

	length = DotProduct(v, v);
	length = sqrt(length);		/** @todo */

	if (!equal(length, 0.0)) {
		const float ilength = 1.0 / length;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
	}

	return length;
}

/**
 * @brief Sets vector_out (vc) to vevtor1 (va) + scale * vector2 (vb)
 * @param[in] veca Position to start from
 * @param[in] scale Speed of the movement
 * @param[in] vecb Movement direction
 * @param[out] outVector Target vector
 */
void VectorMA (const vec3_t veca, const float scale, const vec3_t vecb, vec3_t outVector)
{
	outVector[0] = veca[0] + scale * vecb[0];
	outVector[1] = veca[1] + scale * vecb[1];
	outVector[2] = veca[2] + scale * vecb[2];
}

void VectorClampMA (vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc)
{
	int i;

	/* clamp veca to bounds */
	for (i = 0; i < 3; i++)
		if (veca[i] > 4094.0)
			veca[i] = 4094.0;
		else if (veca[i] < -4094.0)
			veca[i] = -4094.0;

	/* rescale to fit */
	for (i = 0; i < 3; i++) {
		const float test = veca[i] + scale * vecb[i];
		if (test < -4095.0f) {
			const float newScale = (-4094.0 - veca[i]) / vecb[i];
			if (fabs(newScale) < fabs(scale))
				scale = newScale;
		} else if (test > 4095.0f) {
			const float newScale = (4094.0 - veca[i]) / vecb[i];
			if (fabs(newScale) < fabs(scale))
				scale = newScale;
		}
	}

	/* use rescaled scale */
	VectorMA(veca, scale, vecb, vecc);
}

/**
 * @brief Multiply 3*3 matrix by 3*3 matrix.
 * @param[out] c The result of the multiplication matrix = @c a * @c b (not the same as @c b * @c a !)
 * @param[in] a First matrix.
 * @param[in] b Second matrix.
 * @sa GLMatrixMultiply
 */
void MatrixMultiply (const vec3_t a[3], const vec3_t b[3], vec3_t c[3])
{
	c[0][0] = a[0][0] * b[0][0] + a[1][0] * b[0][1] + a[2][0] * b[0][2];
	c[0][1] = a[0][1] * b[0][0] + a[1][1] * b[0][1] + a[2][1] * b[0][2];
	c[0][2] = a[0][2] * b[0][0] + a[1][2] * b[0][1] + a[2][2] * b[0][2];

	c[1][0] = a[0][0] * b[1][0] + a[1][0] * b[1][1] + a[2][0] * b[1][2];
	c[1][1] = a[0][1] * b[1][0] + a[1][1] * b[1][1] + a[2][1] * b[1][2];
	c[1][2] = a[0][2] * b[1][0] + a[1][2] * b[1][1] + a[2][2] * b[1][2];

	c[2][0] = a[0][0] * b[2][0] + a[1][0] * b[2][1] + a[2][0] * b[2][2];
	c[2][1] = a[0][1] * b[2][0] + a[1][1] * b[2][1] + a[2][1] * b[2][2];
	c[2][2] = a[0][2] * b[2][0] + a[1][2] * b[2][1] + a[2][2] * b[2][2];
}

/**
 * @brief Builds an opengl translation and rotation matrix
 * @param origin The translation vector
 * @param angles The angle vector that is used to calculate the rotation part of the matrix
 * @param matrix The resulting matrix, must be of dimension 16
 */
void GLMatrixAssemble (const vec3_t origin, const vec3_t angles, float* matrix)
{
	/* fill in edge values */
	matrix[3] = matrix[7] = matrix[11] = 0.0;
	matrix[15] = 1.0;

	/* add rotation */
	AngleVectors(angles, &matrix[0], &matrix[4], &matrix[8]);
	/* flip an axis */
	VectorInverse(&matrix[4]);

	/* add translation */
	matrix[12] = origin[0];
	matrix[13] = origin[1];
	matrix[14] = origin[2];
}

/**
 * @brief Multiply 4*4 matrix by 4*4 matrix.
 * @sa MatrixMultiply
 * @param[out] c The result of the multiplication matrix = @c a * @c b (not the same as @c b * @c a as matrix
 * multiplication is not commutative)
 * @param[in] a First matrix.
 * @param[in] b Second matrix.
 */
void GLMatrixMultiply (const float a[16], const float b[16], float c[16])
{
	int i, j, k;

	for (j = 0; j < 4; j++) {
		k = j * 4;
		for (i = 0; i < 4; i++)
			c[i + k] = a[i] * b[k] + a[i + 4] * b[k + 1] + a[i + 8] * b[k + 2] + a[i + 12] * b[k + 3];
	}
}

/**
 * @brief Multiply 4*4 matrix by 4d vector.
 * @param[out] out the result of the multiplication = @c m * @c in.
 * @param[in] m 4*4 matrix
 * @param[in] in 4d vector.
 * @sa VectorRotate
 */
void GLVectorTransform (const float m[16], const vec4_t in, vec4_t out)
{
	int i;

	for (i = 0; i < 4; i++)
		out[i] = m[i] * in[0] + m[i + 4] * in[1] + m[i + 8] * in[2] + m[i + 12] * in[3];
}

/**
 * @brief Transform position (xyz) vector by OpenGL rules
 * @note Equivalent to calling GLVectorTransfrom with (x y z 1) vector and taking first 3 components of result
 * @param[out] out the result of the multiplication = @c m * @c in.
 * @param[in] m 4*4 matrix
 * @param[in] in 3d vector.
 * @sa GLVectorTransform
 */
void GLPositionTransform (const float m[16], const vec3_t in, vec3_t out)
{
	int i;

	for (i = 0; i < 3; i++)
		out[i] = m[i] * in[0] + m[i + 4] * in[1] + m[i + 8] * in[2] + m[i + 12];
}

/**
 * @brief Rotate a vector with a rotation matrix.
 * @param[out] vb The result of multiplication (ie vector @c va after rotation).
 * @param[in] m The 3*3 matrix (rotation matrix in case of a rotation).
 * @param[in] va The vector that should be multiplied (ie rotated in case of rotation).
 * @note Basically, this is just the multiplication of @c m * @c va: this is the same than
 * GLVectorTransform in 3D. This can be used for other applications than rotation.
 * @sa GLVectorTransform
 */
void VectorRotate (vec3_t m[3], const vec3_t va, vec3_t vb)
{
	vb[0] = m[0][0] * va[0] + m[1][0] * va[1] + m[2][0] * va[2];
	vb[1] = m[0][1] * va[0] + m[1][1] * va[1] + m[2][1] * va[2];
	vb[2] = m[0][2] * va[0] + m[1][2] * va[1] + m[2][2] * va[2];
}

/**
 * @brief Compare two vectors that may have an epsilon difference but still be
 * the same vectors
 * @param[in] v1 vector to compare with v2
 * @param[in] v2 vector to compare with v1
 * @param[in] epsilon The epsilon the vectors may differ to still be the same
 * @return 1 if the 2 vectors are the same (less than epsilon difference).
 * @note This is not an exact calculation (should use a sqrt). Use this function
 * only if you want to know if both vectors are the same with a precison that is
 * roughly epsilon (the difference should be lower than sqrt(3) * epsilon).
 */
int VectorCompareEps (const vec3_t v1, const vec3_t v2, float epsilon)
{
	vec3_t d;

	VectorSubtract(v1, v2, d);
	d[0] = fabs(d[0]);
	d[1] = fabs(d[1]);
	d[2] = fabs(d[2]);

	if (d[0] > epsilon || d[1] > epsilon || d[2] > epsilon)
		return 0;

	return 1;
}

/**
 * @brief Calculate the length of a vector
 * @param[in] v Vector to calculate the length of
 * @sa VectorNormalize
 * @return Vector length as vec_t
 */
vec_t VectorLength (const vec3_t v)
{
	return sqrtf(DotProduct(v, v));
}

/**
 * @brief Calculate a position on @c v1 @c v2 line.
 * @param[in] v1 First point of the line.
 * @param[in] v2 Second point of the line.
 * @param[in] mix Position on the line. If 0 < @c mix < 1, @c out is between @c v1 and @c v2 .
 * if @c mix < 0, @c out is outside @c v1 and @c v2 , on @c v1 side.
 * @param[out] out The resulting point
 */
void VectorMix (const vec3_t v1, const vec3_t v2, float mix, vec3_t out)
{
	const float number = 1.0 - mix;

	out[0] = v1[0] * number + v2[0] * mix;
	out[1] = v1[1] * number + v2[1] * mix;
	out[2] = v1[2] * number + v2[2] * mix;
}

/**
 * @brief Inverse a vector.
 * @param[in,out] v Vector to inverse. Output value is -@c v.
 */
void VectorInverse (vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

/**
 * @brief Calculates the midpoint between two vectors.
 * @param[in] point1 vector of first point.
 * @param[in] point2 vector of second point.
 * @param[out] midpoint calculated midpoint vector.
 */
void VectorMidpoint (const vec3_t point1, const vec3_t point2, vec3_t midpoint)
{
	VectorAdd(point1, point2, midpoint);
	VectorScale(midpoint, 0.5f, midpoint);
}

/**
 * @brief Calculates the angle (in radians) between the two given vectors.
 * @note Both vectors must be normalized.
 * @return the angle in radians.
 */
float VectorAngleBetween (const vec3_t vec1, const vec3_t vec2)
{
	const float dot = DotProduct(vec1, vec2);
	const float angle = acos(dot);
	return angle;
}


int Q_log2 (int val)
{
	int answer = 0;

	while (val >>= 1)
		answer++;
	return answer;
}

/**
 * @brief Return random values between 0 and 1
 * @sa crand
 * @sa gaussrand
 */
float frand (void)
{
	return (rand() & 32767) * (1.0 / 32767);
}


/**
 * @brief Return random values between -1 and 1
 * @sa frand
 * @sa gaussrand
 */
float crand (void)
{
	return (rand() & 32767) * (2.0 / 32767) - 1;
}

/**
 * @brief generate two gaussian distributed random numbers with median at 0 and stdev of 1
 * @param[out] gauss1 First gaussian distributed random number
 * @param[out] gauss2 Second gaussian distributed random number
 * @sa crand
 * @sa frand
 */
void gaussrand (float *gauss1, float *gauss2)
{
	float x1, x2, w, tmp;

	do {
		x1 = crand();
		x2 = crand();
		w = x1 * x1 + x2 * x2;
	} while (w >= 1.0);

	tmp = -2 * logf(w);
	w = sqrt(tmp / w);
	*gauss1 = x1 * w;
	*gauss2 = x2 * w;
}

void CalculateMinsMaxs (const vec3_t angles, const vec3_t mins, const vec3_t maxs, const vec3_t origin, vec3_t absmin, vec3_t absmax)
{
	/* expand for rotation */
	if (VectorNotEmpty(angles)) {
		vec3_t minVec, maxVec, tmpMinVec, tmpMaxVec;
		vec3_t centerVec, halfVec, newCenterVec, newHalfVec;
		vec3_t m[3];

		/* Find the center of the extents. */
		VectorCenterFromMinsMaxs(mins, maxs, centerVec);

		/* Find the half height and half width of the extents. */
		VectorSubtract(maxs, centerVec, halfVec);

		/* Rotate the center about the origin. */
		VectorCreateRotationMatrix(angles, m);
		VectorRotate(m, centerVec, newCenterVec);
		VectorRotate(m, halfVec, newHalfVec);

		/* Set minVec and maxVec to bound around newCenterVec at halfVec size. */
		VectorSubtract(newCenterVec, newHalfVec, tmpMinVec);
		VectorAdd(newCenterVec, newHalfVec, tmpMaxVec);

		/* rotation may have changed min and max of the box, so adjust it */
		minVec[0] = min(tmpMinVec[0], tmpMaxVec[0]);
		minVec[1] = min(tmpMinVec[1], tmpMaxVec[1]);
		minVec[2] = min(tmpMinVec[2], tmpMaxVec[2]);
		maxVec[0] = max(tmpMinVec[0], tmpMaxVec[0]);
		maxVec[1] = max(tmpMinVec[1], tmpMaxVec[1]);
		maxVec[2] = max(tmpMinVec[2], tmpMaxVec[2]);

		/* Adjust the absolute mins/maxs */
		VectorAdd(origin, minVec, absmin);
		VectorAdd(origin, maxVec, absmax);
	} else {  /* normal */
		VectorAdd(origin, mins, absmin);
		VectorAdd(origin, maxs, absmax);
	}
}

/**
 * @param angles The angles to calulcate the rotation matrix for
 * @param matrix The resulting rotation matrix. The @c right part of this matrix is inversed because
 * of the coordinate system we are using internally.
 * @see AnglesVectors
 * @see VectorRotatePoint
 */
void VectorCreateRotationMatrix (const vec3_t angles, vec3_t matrix[3])
{
	AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
	VectorInverse(matrix[1]);
}

/**
 * @param[out] point The vector to rotate around and the location of the rotated vector
 * @param[in] matrix The input rotation matrix
 * @see VectorCreateRotationMatrix
 */
void VectorRotatePoint (vec3_t point, vec3_t matrix[3])
{
	vec3_t tvec;

	VectorCopy(point, tvec);

	point[0] = DotProduct(matrix[0], tvec);
	point[1] = DotProduct(matrix[1], tvec);
	point[2] = DotProduct(matrix[2], tvec);
}

/**
 * @brief Create the rotation matrix in order to rotate something.
 * @param[in] angles Contains the three angles PITCH, YAW and ROLL (in degree) of rotation around idle
 * frame ({0, 1, 0}, {0, 0, 1} ,{1, 0, 0}) (in this order!)
 * @param[out] forward return the first line of the rotation matrix.
 * @param[out] right return the second line of the rotation matrix.
 * @param[out] up return the third line of the rotation matrix.
 * @note This matrix is the product of the 3 matrixes R*P*Y (in this order!), where R is the rotation matrix around {1, 0, 0} only
 * (angle of rotation is angle[2]), P is the rotation matrix around {0, 1, 0} only, and Y is the rotation matrix around {0, 0, 1}.
 * @note Due to z convention for Quake, the z-axis is inverted. Therefore, if you want to use this function in a direct frame, don't
 * forget to inverse the second line (@c right).
 * Exemple : to rotate v2 into v :
 * AngleVectors(angles, m[0], m[1], m[2]);
 * VectorInverse(m[1]);
 * VectorRotate(m, v2, v);
 * @sa RotatePointAroundVector : If you need rather "one rotation around a vector you choose" instead of "3 rotations around 3 vectors you don't choose".
 */
void AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	const float anglePitch = angles[PITCH] * torad;
	const float sp = sin(anglePitch);
	const float cp = cos(anglePitch);
	const float angleYaw = angles[YAW] * torad;
	const float sy = sin(angleYaw);
	const float cy = cos(angleYaw);
	const float angleRoll = angles[ROLL] * torad;
	const float sr = sin(angleRoll);
	const float cr = cos(angleRoll);

	if (forward) {
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if (right) {
		right[0] = (-1 * sr * sp * cy + -1 * cr * -sy);
		right[1] = (-1 * sr * sp * sy + -1 * cr * cy);
		right[2] = -1 * sr * cp;
	}
	if (up) {
		up[0] = (cr * sp * cy + -sr * -sy);
		up[1] = (cr * sp * sy + -sr * cy);
		up[2] = cr * cp;
	}
}

/**
 * @brief Checks whether a point is visible from a given position
 * @param[in] origin Origin to test from
 * @param[in] dir Direction to test into
 * @param[in] point This is the point we want to check the visibility for
 */
qboolean FrustumVis (const vec3_t origin, int dir, const vec3_t point)
{
	/* view frustum check */
	vec3_t delta;
	byte dv;

	delta[0] = point[0] - origin[0];
	delta[1] = point[1] - origin[1];
	delta[2] = 0;
	VectorNormalizeFast(delta);
	dv = dir & (DIRECTIONS - 1);

	/* test 120 frustum (cos 60 = 0.5) */
	if ((delta[0] * dvecsn[dv][0] + delta[1] * dvecsn[dv][1]) < 0.5)
		return qfalse;
	else
		return qtrue;
}

/**
 * @brief Projects a point on a plane passing through the origin
 * @param[in] point Coordinates of the point to project
 * @param[in] normal The normal vector of the plane
 * @param[out] dst Coordinates of the projection on the plane
 * @pre @c Non-null pointers and a normalized normal vector.
 */
static inline void ProjectPointOnPlane (vec3_t dst, const vec3_t point, const vec3_t normal)
{
	float distance; /**< closest distance from the point to the plane */

#if 0
	vec3_t n;
	float inv_denom;
	/* I added a sqrt there, otherwise this function does not work for unnormalized vector (13052007 Kracken) */
	/* old line was inv_denom = 1.0F / DotProduct(normal, normal); */
	inv_denom = 1.0F / sqrt(DotProduct(normal, normal));
#endif

	distance = DotProduct(normal, point);
#if 0
	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;
#endif

	dst[0] = point[0] - distance * normal[0];
	dst[1] = point[1] - distance * normal[1];
	dst[2] = point[2] - distance * normal[2];
}

static inline float Q_rsqrtApprox (const float number)
{
	union
	{
		float f;
		int i;
	} t;
	float y;
	float x2;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	t.f = number;
	/* what the fuck? */
	t.i = 0x5f3759df - (t.i >> 1);
	y = t.f;
	/* 1st iteration */
	y = y * (threehalfs - (x2 * y * y));
	/* 2nd iteration */
	y = y * (threehalfs - (x2 * y * y));
	return y;
}

/**
 * @brief Calculate unit vector for a given vec3_t
 * @param[in] v Vector to normalize
 * @sa VectorNormalize2
 * @return vector length as vec_t
 */
vec_t VectorNormalize (vec3_t v)
{
	const float length = sqrt(DotProduct(v, v));
	if (length) {
		const float ilength = 1.0 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

/**
 * @brief fast vector normalize routine that does not check to make sure
 * that length != 0, nor does it return length
 */
void VectorNormalizeFast (vec3_t v)
{
	const float ilength = Q_rsqrtApprox(DotProduct(v, v));
	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

/**
 * @brief Finds a vector perpendicular to the source vector
 * @param[in] src The source vector
 * @param[out] dst A vector perpendicular to @c src
 * @note @c dst is a perpendicular vector to @c src such that it is the closest
 * to one of the three axis: {1,0,0}, {0,1,0} and {0,0,1} (chosen in that order
 * in case of equality)
 * @pre non-NULL pointers and @c src is normalized
 * @sa ProjectPointOnPlane
 */
void PerpendicularVector (vec3_t dst, const vec3_t src)
{
	int pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	/* find the smallest magnitude axially aligned vector */
	for (pos = 0, i = 0; i < 3; i++) {
		const float a = fabs(src[i]);
		if (a < minelem) {
			pos = i;
			minelem = a;
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/* project the point onto the plane defined by src */
	ProjectPointOnPlane(dst, tempvec, src);

	/* normalize the result */
	VectorNormalizeFast(dst);
}

/**
 * @brief binary operation on vectors in a three-dimensional space
 * @note also known as the vector product or outer product
 * @note It differs from the dot product in that it results in a vector
 * rather than in a scalar
 * @note Its main use lies in the fact that the cross product of two vectors
 * is orthogonal to both of them.
 * @param[in] v1 directional vector
 * @param[in] v2 directional vector
 * @param[out] cross output
 * @note you have the right and forward values of an axis, their cross product will
 * @note be a properly oriented "up" direction
 * @sa DotProduct
 * @sa VectorNormalize2
 */
void CrossProduct (const vec3_t v1, const vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

static inline void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
}

/**
 * @brief Rotate a point around a given vector
 * @param[in] dir The vector around which to rotate
 * @param[in] point The point to be rotated
 * @param[in] degrees How many degrees to rotate the point by
 * @param[out] dst The point after rotation
 * @note Warning: @c dst must be different from @c point (otherwise the result has no meaning)
 * @pre @c dir must be normalized
 */
void RotatePointAroundVector (vec3_t dst, const vec3_t dir, const vec3_t point, float degrees)
{
	float m[3][3];
	float im[3][3];
	float zrot[3][3];
	float tmpmat[3][3];
	float rot[3][3];
	int i;
	vec3_t vr, vup, vf;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector(vr, dir);
	CrossProduct(vr, vf, vup);

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy(im, m, sizeof(im));

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	OBJZERO(zrot);

	/* now prepare the rotation matrix */
	zrot[0][0] = cos(degrees * torad);
	zrot[0][1] = sin(degrees * torad);
	zrot[1][0] = -sin(degrees * torad);
	zrot[1][1] = cos(degrees * torad);
	zrot[2][2] = 1.0F;

	R_ConcatRotations(m, zrot, tmpmat);
	R_ConcatRotations(tmpmat, im, rot);

	for (i = 0; i < 3; i++) {
		dst[i] = DotProduct(rot[i], point);
	}
}

/**
 * @brief Print a 3D vector
 * @param[in] v The vector to be printed
 */
void Print3Vector (const vec3_t v, const char *text)
{
	Com_Printf("%s (%f, %f, %f)\n", text, v[0], v[1], v[2]);
}

/**
 * @brief Print a 2D vector
 * @param[in] v The vector to be printed
 */
void Print2Vector (const vec2_t v, const char *text)
{
	Com_Printf("%s (%f, %f)\n", text, v[0], v[1]);
}

/**
 * @brief Converts longitude and latitude to a 3D vector in Euclidean
 * coordinates
 * @param[in] a The longitude and latitude in a 2D vector
 * @param[out] v The resulting normal 3D vector
 * @sa VecToPolar
 */
void PolarToVec (const vec2_t a, vec3_t v)
{
	const float p = a[0] * torad;	/* long */
	const float t = a[1] * torad;	/* lat */
	/* v[0] = z, v[1] = x, v[2] = y - wtf? */
	VectorSet(v, cos(p) * cos(t), sin(p) * cos(t), sin(t));
}

/**
 * @brief Converts vector coordinates into polar coordinates
 * @sa PolarToVec
 */
void VecToPolar (const vec3_t v, vec2_t a)
{
	a[0] = todeg * atan2(v[1], v[0]);	/* long */
	a[1] = 90 - todeg * acos(v[2]);	/* lat */
}

/**
 * @brief Converts a vector to an angle vector
 * @param[in] value1
 * @param[in] angles Target vector for pitch, yaw, roll
 * @sa anglemod
 */
void VecToAngles (const vec3_t value1, vec3_t angles)
{
	float yaw, pitch;

	/* only check the first two values for being zero */
	if (Vector2Empty(value1)) {
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90.0;
		else
			pitch = 270.0;
	} else {
		const float forward = sqrt(value1[0] * value1[0] + value1[1] * value1[1]);
		if (!equal(value1[0], 0.0))
			yaw = (int) (atan2(value1[1], value1[0]) * todeg);
		else if (value1[1] > 0)
			yaw = 90.0;
		else
			yaw = -90.0;
		if (yaw < 0.0)
			yaw += 360.0;

		pitch = (int) (atan2(value1[2], forward) * todeg);
		if (pitch < 0)
			pitch += 360.0;
	}

	/* up and down */
	angles[PITCH] = -pitch;
	/* left and right */
	angles[YAW] = yaw;
	/* tilt left and right */
	angles[ROLL] = 0;
}


/**
 * @brief Checks whether i is power of two value
 */
qboolean Q_IsPowerOfTwo (int i)
{
	return (i > 0 && !(i & (i - 1)));
}

/**
 * @brief Returns the angle resulting from turning fraction * angle
 * from angle1 to angle2
 */
float LerpAngle (float a2, float a1, float frac)
{
	if (a1 - a2 > 180)
		a1 -= 360;
	if (a1 - a2 < -180)
		a1 += 360;
	return a2 + frac * (a1 - a2);
}

/**
 * @brief returns angle normalized to the range [0 <= angle < 360]
 * @param[in] angle Angle
 * @sa VecToAngles
 */
float AngleNormalize360 (float angle)
{
	return (360.0 / 65536) * ((int)(angle * (65536 / 360.0)) & 65535);
}

/**
 * @brief returns angle normalized to the range [-180 < angle <= 180]
 * @param[in] angle Angle
 */
float AngleNormalize180 (float angle)
{
	angle = AngleNormalize360(angle);
	if (angle > 180.0)
		angle -= 360.0;
	return angle;
}

/**
 * @brief Calculates the center of a bounding box
 * @param[in] mins The lower end of the bounding box
 * @param[in] maxs The upper end of the bounding box
 * @param[out] center The target center vector calculated from @c mins and @c maxs
 */
void VectorCenterFromMinsMaxs (const vec3_t mins, const vec3_t maxs, vec3_t center)
{
	VectorAdd(mins, maxs, center);
	VectorScale(center, 0.5, center);
}

/**
 * @brief Calculates a bounding box from a center and a size
 * @param[in] center The center vector
 * @param[in] size The size vector to calculate the bbox
 * @param[out] mins The lower end of the bounding box
 * @param[out] maxs The upper end of the bounding box
 */
void VectorCalcMinsMaxs (const vec3_t center, const vec3_t size, vec3_t mins, vec3_t maxs)
{
	int i;

	for (i = 0; i < 3; i++) {
		const vec_t length = abs(size[i]) / 2;
		mins[i] = center[i] - length;
		maxs[i] = center[i] + length;
	}
}

/**
 * @brief Sets mins and maxs to their starting points before using AddPointToBounds
 * @sa AddPointToBounds
 */
void ClearBounds (vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

/**
 * @brief If the point is outside the box defined by mins and maxs, expand
 * the box to accommodate it. Sets mins and maxs to their new values
 */
void AddPointToBounds (const vec3_t v, vec3_t mins, vec3_t maxs)
{
	int i;
	vec_t val;

	for (i = 0; i < 3; i++) {
		val = v[i];
		if (val < mins[i])
			mins[i] = val;
		if (val > maxs[i])
			maxs[i] = val;
	}
}

/**
 * @brief Projects the normalized directional vectors on to the normal's plane.
 * The fourth component of the resulting tangent vector represents sidedness.
 */
void TangentVectors (const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec4_t tangent, vec3_t binormal)
{
	vec3_t s, t;

	/* normalize the directional vectors */
	VectorCopy(sdir, s);
	VectorNormalizeFast(s);

	VectorCopy(tdir, t);
	VectorNormalizeFast(t);

	/* project the directional vector onto the plane */
	VectorMA(s, -DotProduct(s, normal), normal, tangent);
	VectorNormalizeFast(tangent);

	/* resolve sidedness, encode as fourth tangent component */
	CrossProduct(normal, tangent, binormal);

	if (DotProduct(t, binormal) < 0.0)
		tangent[3] = -1.0;
	else
		tangent[3] = 1.0;

	VectorScale(binormal, tangent[3], binormal);
}

/**
 * @brief Grahm-Schmidt orthogonalization
 * @param[out] out Orthogonalized vector
 * @param[in] in Reference vector
 */
void Orthogonalize (vec3_t out, const vec3_t in)
{
	vec3_t tmp;
	VectorMul(DotProduct(out, in), in, tmp);
	VectorSubtract(out, tmp, out);
	VectorNormalizeFast(out);
}

/**
 * @brief Transposes @c m and stores the result in @c t
 * @param[in] m The matrix to transpose
 * @param[out] t The transposed matrix
 */
void MatrixTranspose (const vec3_t m[3], vec3_t t[3])
{
	int i, j;

	for (i = 0; i < 3; i++) {
		for(j = 0; j < 3; j++) {
			t[i][j] = m[j][i];
		}
	}
}

qboolean RayIntersectAABB (const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs)
{
	float t0 = 0.0f;
	float t1 = 1.0f;
	vec3_t delta;
	int i;

	VectorSubtract(end, start, delta);

	for (i = 0; i < 3; i++) {
		const float threshold = 1.0e-6f;
		float u0, u1;

		if (fabs(delta[i]) < threshold) {
			if (delta[i] > 0.0f) {
				return !(end[i] < mins[i] || start[i] > maxs[i]);
			} else {
				return !(start[i] < mins[i] || end[i] > maxs[i]);
			}
		}

		u0 = (mins[i] - start[i]) / delta[i];
		u1 = (maxs[i] - start[i]) / delta[i];

		if (u0 > u1) {
			const float temp = u0;
			u0 = u1;
			u1 = temp;
		}

		if (u1 < t0 || u0 > t1) {
			return qfalse;
		}

		t0 = max(u0, t0);
		t1 = min(u1, t1);

		if (t1 < t0) {
			return qfalse;
		}
	}

	return qtrue;
}
