#ifndef __MATHLIB__
#define __MATHLIB__

/* mathlib.h */

#include <math.h>

#ifdef DOUBLEVEC_T
typedef double vec_t;
#else
typedef float vec_t;
#endif
typedef vec_t vec3_t[3];

#ifndef M_PI
#define M_PI        3.14159265358979323846  /* matches value in gcc v2 math.h */
#endif

#define	SIDE_FRONT		0
#define	SIDE_ON			2
#define	SIDE_BACK		1
#define	SIDE_CROSS		-2

extern const vec3_t vec3_origin;

#define	EQUAL_EPSILON	0.001

qboolean VectorCompare(const vec3_t v1, const vec3_t v2);
qboolean VectorNearer(const vec3_t v1, const vec3_t v2, const vec3_t comp);

#define DotProduct(x,y) (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c) {c[0]=a[0]-b[0];c[1]=a[1]-b[1];c[2]=a[2]-b[2];}
#define VectorAdd(a,b,c) {c[0]=a[0]+b[0];c[1]=a[1]+b[1];c[2]=a[2]+b[2];}
#define VectorCopy(a,b) {b[0]=a[0];b[1]=a[1];b[2]=a[2];}
#define VectorScale(a,b,c) {c[0]=b*a[0];c[1]=b*a[1];c[2]=b*a[2];}
#define VectorClear(x) {x[0] = x[1] = x[2] = 0;}
#define	VectorNegate(x) {x[0]=-x[0];x[1]=-x[1];x[2]=-x[2];}

vec_t Q_rint(const vec_t in);
vec_t _DotProduct(const vec3_t v1const , vec3_t v2);
void _VectorSubtract(const vec3_t va, const vec3_t vb, vec3_t out);
void _VectorAdd(const vec3_t va, const vec3_t vb, vec3_t out);
void _VectorCopy(const vec3_t in, vec3_t out);
void _VectorScale(const vec3_t v, const vec_t scale, vec3_t out);

double VectorLength(const vec3_t v);

void VectorMA(const vec3_t va, const vec_t scale, const vec3_t vb, vec3_t vc);

void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross);
vec_t VectorNormalize(const vec3_t in, vec3_t out);
vec_t ColorNormalize(const vec3_t in, vec3_t out);
void VectorInverse(vec3_t v);

void ClearBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs);

#endif
