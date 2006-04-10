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

/* FIXME:
** there is a bug for ic->next - these loops (while and for loops) are sometimes endless-loops
**/

#include "q_shared.h"

#define DEG2RAD( a ) (( a * M_PI ) / 180.0F)

vec3_t vec3_origin = {0,0,0};
vec4_t vec4_origin = {0,0,0,0};

#define RT2	0.707107

const int dvecs[8][2] = { {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,-1}, {-1,1}, {1,-1} };
const float dvecsn[8][2] = { {1,0}, {-1,0}, {0,1}, {0,-1}, {RT2,RT2}, {-RT2,-RT2}, {-RT2,RT2}, {RT2,-RT2} };
const float dangle[8] = { 0, 180.0f, 90.0f, 270.0f, 45.0f, 225.0f, 135.0f, 315.0f };

const byte dvright[8] = { 7, 6, 4, 5, 0, 1, 2, 3 };
const byte dvleft[8]  = { 4, 5, 6, 7, 2, 3, 1, 0 };

//============================================================================

int AngleToDV( int angle )
{
	int	i, mini;
	int	div, minDiv;

	angle %= 360;
	while ( angle > 360 - 22 ) angle -= 360;
	while ( angle < - 22 ) angle += 360;
	minDiv = 360;
	mini = 0;

	for ( i = 0; i < 8; i++ )
	{
		div = angle - dangle[i];
		div = (div < 0) ? -div : div;
		if ( div < minDiv )
		{
			mini = i;
			minDiv = div;
		}
	}

	return mini;
}

//============================================================================

#ifdef _MSC_VER
#pragma optimize( "", off )
#endif

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees )
{
	float	m[3][3];
	float	im[3][3];
	float	zrot[3][3];
	float	tmpmat[3][3];
	float	rot[3][3];
	int	i;
	vec3_t vr, vup, vf;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector( vr, dir );
	CrossProduct( vr, vf, vup );

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy( im, m, sizeof( im ) );

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset( zrot, 0, sizeof( zrot ) );
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	zrot[0][0] = cos( DEG2RAD( degrees ) );
	zrot[0][1] = sin( DEG2RAD( degrees ) );
	zrot[1][0] = -sin( DEG2RAD( degrees ) );
	zrot[1][1] = cos( DEG2RAD( degrees ) );

	R_ConcatRotations( m, zrot, tmpmat );
	R_ConcatRotations( tmpmat, im, rot );

	for ( i = 0; i < 3; i++ )
	{
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	}
}

#ifdef _MSC_VER
#pragma optimize( "", on )
#endif

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float		angle;
	static float		sr, sp, sy, cr, cp, cy;
	// static to help MS compiler fp bugs

	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	if (forward)
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if (right)
	{
		right[0] = (-1*sr*sp*cy+-1*cr*-sy);
		right[1] = (-1*sr*sp*sy+-1*cr*cy);
		right[2] = -1*sr*cp;
	}
	if (up)
	{
		up[0] = (cr*sp*cy+-sr*-sy);
		up[1] = (cr*sp*sy+-sr*cy);
		up[2] = cr*cp;
	}
}

void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal )
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct( normal, normal );

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	int	pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	// find the smallest magnitude axially aligned vector
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( fabs( src[i] ) < minelem )
		{
			pos = i;
			minelem = fabs( src[i] );
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	// project the point onto the plane defined by src
	ProjectPointOnPlane( dst, tempvec, src );

	// normalize the result
	VectorNormalize( dst );
}



/*
================
R_ConcatRotations
================
*/
void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
}


/*
================
R_ConcatTransforms
================
*/
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
				in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
				in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
				in1[2][2] * in2[2][3] + in1[2][3];
}


void VecToAngles (vec3_t value1, vec3_t angles)
{
	float	forward;
	float	yaw, pitch;

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		if (value1[0])
			yaw = (int) (atan2(value1[1], value1[0]) * 180 / M_PI);
		else if (value1[1] > 0)
			yaw = 90;
		else
			yaw = -90;
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (int) (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}


//============================================================================


float Q_fabs (float f)
{
#if 0
	if (f >= 0)
		return f;
	return -f;
#else
	int tmp = * ( int * ) &f;
	tmp &= 0x7FFFFFFF;
	return * ( float * ) &tmp;
#endif
}

#if defined _M_IX86 && !defined C_ONLY
#pragma warning (disable:4035)
__declspec( naked ) long Q_ftol( float f )
{
	static int tmp;
	__asm fld dword ptr [esp+4]
	__asm fistp tmp
	__asm mov eax, tmp
	__asm ret
}
#pragma warning (default:4035)
#endif

/*
===============
LerpAngle

===============
*/
float LerpAngle (float a2, float a1, float frac)
{
	if (a1 - a2 > 180)
		a1 -= 360;
	if (a1 - a2 < -180)
		a1 += 360;
	return a2 + frac * (a1 - a2);
}


float	anglemod(float a)
{
#if 0
	if (a >= 0)
		a -= 360*(int)(a/360);
	else
		a += 360*( 1 + (int)(-a/360) );
#endif
	a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}

int	i;
vec3_t	corners[2];

// this is the slow, general version
int BoxOnPlaneSide2 (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	int		i;
	float	dist1, dist2;
	int		sides;
	vec3_t	corners[2];

	for (i=0 ; i<3 ; i++)
	{
		if (p->normal[i] < 0)
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist1 = DotProduct (p->normal, corners[0]) - p->dist;
	dist2 = DotProduct (p->normal, corners[1]) - p->dist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;

	return sides;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
#if !id386 || defined __linux__ || __MINGW32__ || defined __FreeBSD__ || defined __sun || defined __sgi
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	float	dist1, dist2;
	int		sides;

	// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}

	// general case
	switch (p->signbits)
	{
	case 0:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 1:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 2:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 3:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 4:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 5:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 6:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	case 7:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	default:
		dist1 = dist2 = 0;		// shut up compiler
		assert( 0 );
		break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

	assert( sides != 0 );

	return sides;
}
#else
#pragma warning( disable: 4035 )

__declspec( naked ) int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	static int bops_initialized;
	static int Ljmptab[8];

	__asm {

		push ebx

		cmp bops_initialized, 1
		je  initialized
		mov bops_initialized, 1

		mov Ljmptab[0*4], offset Lcase0
		mov Ljmptab[1*4], offset Lcase1
		mov Ljmptab[2*4], offset Lcase2
		mov Ljmptab[3*4], offset Lcase3
		mov Ljmptab[4*4], offset Lcase4
		mov Ljmptab[5*4], offset Lcase5
		mov Ljmptab[6*4], offset Lcase6
		mov Ljmptab[7*4], offset Lcase7

initialized:

		mov edx,ds:dword ptr[4+12+esp]
		mov ecx,ds:dword ptr[4+4+esp]
		xor eax,eax
		mov ebx,ds:dword ptr[4+8+esp]
		mov al,ds:byte ptr[17+edx]
		cmp al,8
		jge Lerror
		fld ds:dword ptr[0+edx]
		fld st(0)
		jmp dword ptr[Ljmptab+eax*4]
Lcase0:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase1:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase2:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase3:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase4:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase5:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase6:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase7:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
LSetSides:
		faddp st(2),st(0)
		fcomp ds:dword ptr[12+edx]
		xor ecx,ecx
		fnstsw ax
		fcomp ds:dword ptr[12+edx]
		and ah,1
		xor ah,1
		add cl,ah
		fnstsw ax
		and ah,1
		add ah,ah
		add cl,ah
		pop ebx
		mov eax,ecx
		ret
Lerror:
		int 3
	}
}
#pragma warning( default: 4035 )
#endif

void ClearBounds (vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs)
{
	int		i;
	vec_t	val;

	for (i=0 ; i<3 ; i++)
	{
		val = v[i];
		if (val < mins[i])
			mins[i] = val;
		if (val > maxs[i])
			maxs[i] = val;
	}
}


qboolean VectorNearer (vec3_t v1, vec3_t v2, vec3_t comp)
{
	int		i;

	for (i=0 ; i<3 ; i++)
		if ( fabs(v1[i]-comp[i]) < fabs(v2[i]-comp[i]) )
			return true;

	return false;
}


vec_t VectorNormalize (vec3_t v)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;

}

vec_t VectorNormalize2 (vec3_t v, vec3_t out)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1/length;
		out[0] = v[0]*ilength;
		out[1] = v[1]*ilength;
		out[2] = v[2]*ilength;
	}

	return length;

}

void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}

void VectorClampMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	float test, newScale;
	int i;
	newScale = scale;

	// clamp veca to bounds
	for ( i = 0; i < 3; i++ )
		if ( veca[i] > 4094.0 ) veca[i] = 4094.0;
		else if ( veca[i] < -4094.0 ) veca[i] = -4094.0;

	// rescale to fit
	for ( i = 0; i < 3; i++ )
	{
		test = veca[i] + scale*vecb[i];
		if ( test < -4095.0f )
		{
			newScale = (-4094.0 - veca[i]) / vecb[i];
			if ( fabs(newScale) < fabs(scale) ) scale = newScale;
		}
		else if ( test > 4095.0f )
		{
			newScale = (4094.0 - veca[i]) / vecb[i];
			if ( fabs(newScale) < fabs(scale) ) scale = newScale;
		}
	}

	// use rescaled scale
	for ( i = 0; i < 3; i++ )
		vecc[i] = veca[i] + scale*vecb[i];
}


void MatrixMultiply (vec3_t a[3], vec3_t b[3], vec3_t c[3])
{
	c[0][0] = a[0][0]*b[0][0] + a[1][0]*b[0][1] + a[2][0]*b[0][2];
	c[0][1] = a[0][1]*b[0][0] + a[1][1]*b[0][1] + a[2][1]*b[0][2];
	c[0][2] = a[0][2]*b[0][0] + a[1][2]*b[0][1] + a[2][2]*b[0][2];

	c[1][0] = a[0][0]*b[1][0] + a[1][0]*b[1][1] + a[2][0]*b[1][2];
	c[1][1] = a[0][1]*b[1][0] + a[1][1]*b[1][1] + a[2][1]*b[1][2];
	c[1][2] = a[0][2]*b[1][0] + a[1][2]*b[1][1] + a[2][2]*b[1][2];

	c[2][0] = a[0][0]*b[2][0] + a[1][0]*b[2][1] + a[2][0]*b[2][2];
	c[2][1] = a[0][1]*b[2][0] + a[1][1]*b[2][1] + a[2][1]*b[2][2];
	c[2][2] = a[0][2]*b[2][0] + a[1][2]*b[2][1] + a[2][2]*b[2][2];
}


void GLMatrixMultiply (float a[16], float b[16], float c[16])
{
	int i, j, k;

	for ( j = 0; j < 4; j++ )
	{
		k = j*4;
		for ( i = 0; i < 4; i++ )
			c[i+k] = a[i]*b[k] + a[i+4]*b[k+1] + a[i+8]*b[k+2] + a[i+12]*b[k+3];
	}
}


void GLVectorTransform (float m[16], vec4_t in, vec4_t out)
{
	int i;

	for ( i = 0; i < 4; i++ )
		out[i] = m[i]*in[0] + m[i+4]*in[1] + m[i+8]*in[2] + m[i+12]*in[3];
}


void VectorRotate (vec3_t m[3], vec3_t va, vec3_t vb)
{
	vb[0] = m[0][0]*va[0] + m[1][0]*va[1] + m[2][0]*va[2];
	vb[1] = m[0][1]*va[0] + m[1][1]*va[1] + m[2][1]*va[2];
	vb[2] = m[0][2]*va[0] + m[1][2]*va[1] + m[2][2]*va[2];
}


vec_t _DotProduct (vec3_t v1, vec3_t v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]-vecb[0];
	out[1] = veca[1]-vecb[1];
	out[2] = veca[2]-vecb[2];
}

void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]+vecb[0];
	out[1] = veca[1]+vecb[1];
	out[2] = veca[2]+vecb[2];
}

void _VectorCopy (vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

double sqrt(double x);

vec_t VectorLength(vec3_t v)
{
	int		i;
	float	length;

	length = 0;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = sqrt (length);		// FIXME

	return length;
}

void VectorInverse (vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void VectorScale (vec3_t in, vec_t scale, vec3_t out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}


int Q_log2(int val)
{
	int answer=0;
	while (val>>=1)
		answer++;
	return answer;
}


float	frand(void)
{
	return (rand()&32767)* (1.0/32767);
}


float	crand(void)
{
	return (rand()&32767)* (2.0/32767) - 1;
}


//====================================================================================

/*
============
stradd
============
*/
void stradd( char **str, const char *addStr )
{
	const char *ch;
	for ( ch = addStr; *ch; ch++, (*str)++ ) **str = *ch;
	**str = 0;
}

/*
==================
Com_CharIsOneOfCharset
==================
*/
static qboolean Com_CharIsOneOfCharset( char c, char *set )
{
	int i;

	for( i = 0; i < strlen( set ); i++ )
	{
		if( set[ i ] == c )
			return true;
	}

	return false;
}

/*
==================
Com_SkipCharset
==================
*/
char *Com_SkipCharset( char *s, char *sep )
{
	char    *p = s;

	while( p )
	{
		if( Com_CharIsOneOfCharset( *p, sep ) )
			p++;
		else
			break;
	}

	return p;
}

/*
==================
Com_SkipTokens
==================
*/
char *Com_SkipTokens( char *s, int numTokens, char *sep )
{
	int             sepCount = 0;
	char    *p = s;

	while( sepCount < numTokens )
	{
		if( Com_CharIsOneOfCharset( *p++, sep ) )
		{
			sepCount++;
			while( Com_CharIsOneOfCharset( *p, sep ) )
				p++;
		}
		else if( *p == '\0' )
			break;
	}

	if( sepCount == numTokens )
		return p;
	else
		return s;
}

/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char	*last;

	last = pathname;
	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension (char *in)
{
	static char exten[8];
	int		i;

	while (*in && *in != '.')
		in++;
	if (!*in)
		return "";
	in++;
	for (i=0 ; i<7 && *in ; i++,in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

/*
============
COM_FileBase
============
*/
void COM_FileBase (char *in, char *out)
{
	char *s, *s2;

	s = in + strlen(in) - 1;

	while (s != in && *s != '.')
		s--;

	for (s2 = s ; s2 != in && *s2 != '/' ; s2--)
	;

	if (s-s2 < 2)
		out[0] = 0;
	else
	{
		s--;
		strncpy (out,s2+1, s-s2);
		out[s-s2] = 0;
	}
}

/*
============
COM_FilePath

Returns the path up to, but not including the last /
============
*/
void COM_FilePath (char *in, char *out)
{
	char *s;

	s = in + strlen(in) - 1;

	while (s != in && *s != '/')
		s--;

	strncpy (out,in, s-in);
	out[s-in] = 0;
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, char *extension)
{
	char    *src;
	//
	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	//
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strcat (path, extension);
}

/*
============================================================================
BYTE ORDER FUNCTIONS
============================================================================
*/

qboolean	bigendien;

// can't just use function pointers, or dll linkage can
// mess up when qcommon is included in multiple places
short	(*_BigShort) (short l);
short	(*_LittleShort) (short l);
int	(*_BigLong) (int l);
int	(*_LittleLong) (int l);
float	(*_BigFloat) (float l);
float	(*_LittleFloat) (float l);

short	BigShort(short l){return _BigShort(l);}
short	LittleShort(short l) {return _LittleShort(l);}
int	BigLong (int l) {return _BigLong(l);}
int	LittleLong (int l) {return _LittleLong(l);}
float	BigFloat (float l) {return _BigFloat(l);}
float	LittleFloat (float l) {return _LittleFloat(l);}

short   ShortSwap (short l)
{
	byte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

short	ShortNoSwap (short l)
{
	return l;
}

int    LongSwap (int l)
{
	byte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

int	LongNoSwap (int l)
{
	return l;
}

float FloatSwap (float f)
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;


	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap (float f)
{
	return f;
}

/*
================
Swap_Init
================
*/
void Swap_Init (void)
{
	byte	swaptest[2] = {1,0};

	// set the byte swapping variables in a portable manner
	if ( *(short *)swaptest == 1)
	{
		bigendien = false;
		_BigShort = ShortSwap;
		_LittleShort = ShortNoSwap;
		_BigLong = LongSwap;
		_LittleLong = LongNoSwap;
		_BigFloat = FloatSwap;
		_LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = true;
		_BigShort = ShortNoSwap;
		_LittleShort = ShortSwap;
		_BigLong = LongNoSwap;
		_LittleLong = LongSwap;
		_BigFloat = FloatNoSwap;
		_LittleFloat = FloatSwap;
	}

}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char	*va(char *format, ...)
{
	va_list		argptr;
	static char	string[2048];

	va_start (argptr, format);
#ifndef _WIN32
	vsnprintf (string, 2048, format,argptr);
#else
	vsprintf (string, format,argptr);
#endif
	va_end (argptr);

	return string;
}


char	com_token[MAX_TOKEN_CHARS];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char **data_p)
{
	int		c;
	int		len;
	char	*data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		*data_p = NULL;
		return "";
	}

	// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}
		data++;
	}

	// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS)
	{
// 		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}


/*
=================
COM_EParse
=================
*/
char *COM_EParse( char **text, char *errhead, char *errinfo )
{
	char *token;

	token = COM_Parse( text );
	if ( !*text )
	{
		if ( errinfo )
			Com_Printf( "%s \"%s\")\n", errhead, errinfo );
		else
			Com_Printf( "%s\n", errhead );

		return NULL;
	}

	return token;
}


/*
===============
Com_PageInMemory

===============
*/
int	paged_total;

void Com_PageInMemory (byte *buffer, int size)
{
	int		i;

	for (i=size-1 ; i>0 ; i-=4096)
		paged_total += buffer[i];
}



/*
============================================================================
LIBRARY REPLACEMENT FUNCTIONS
============================================================================
*/

/* PATCH: matt */
/* use our own strncasecmp instead of this implementation */
#ifdef sun

#define Q_strncasecmp(s1, s2, n) (strncasecmp(s1, s2, n))

int Q_stricmp (char *s1, char *s2)
{
        return strcasecmp(s1, s2);
}

#else // sun
// FIXME: replace all Q_stricmp with Q_strcasecmp
int Q_stricmp (char *s1, char *s2)
{
#ifdef _WIN32
	return _stricmp (s1, s2);
#else
	return strcasecmp (s1, s2);
#endif
}

int Q_strncasecmp (char *s1, char *s2, int n)
{
	int		c1, c2;

	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		// strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;		// strings not equal
		}
	} while (c1);

	return 0;		// strings are equal
}
#endif // sun

/*
=============
Q_strncpyz

Safe strncpy that ensures a trailing zero
=============
*/
void Q_strncpyz( char *dest, const char *src, int destsize )
{
	// bk001129 - also NULL dest
	if ( !dest )
		Sys_Error( "Q_strncpyz: NULL dest" );
	if ( !src )
		Sys_Error( "Q_strncpyz: NULL src" );
	if ( destsize < 1 )
		Sys_Error( "Q_strncpyz: destsize < 1" );

	strncpy( dest, src, destsize-1 );
	dest[destsize-1] = 0;
}

// never goes past bounds or leaves without a terminating 0
void Q_strcat( char *dest, int size, const char *src )
{
	int	l1;

	l1 = strlen( dest );
	if ( l1 >= size ) {
		Sys_Error( "Q_strcat: already overflowed" );
	}
	Q_strncpyz( dest + l1, src, size - l1 );
}

int Q_strcasecmp (char *s1, char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}

void Com_sprintf (char *dest, int size, char *fmt, ...)
{
	int		len;
	va_list		argptr;
	static char	bigbuffer[0x10000];

	va_start (argptr,fmt);
#ifndef _WIN32
	len = vsnprintf (bigbuffer,0x10000,fmt,argptr);
#else
	len = vsprintf (bigbuffer,fmt,argptr);
#endif
	va_end (argptr);
	if (len >= size)
		Com_Printf ("Com_sprintf: overflow of %i in %i\n", len, size);
	Q_strncpyz (dest, bigbuffer, size );
}

/*
=====================================================================
INFO STRINGS
=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;

	valueindex ^= 1;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
			*o++ = *s++;
		*o = 0;

		if (!Q_stricmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey (char *s, const char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strstr (key, "\\"))
	{
//		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qboolean Info_Validate (char *s)
{
	if (strstr (s, "\""))
		return false;
	if (strstr (s, ";"))
		return false;
	return true;
}

void Info_SetValueForKey (char *s, const char *key, const char *value)
{
	char	newi[MAX_INFO_STRING];
// 	char	*v;
// 	int	c;
// 	int	maxsize = MAX_INFO_STRING;

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, ";") )
	{
		Com_Printf ("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > MAX_INFO_KEY-1 || strlen(value) > MAX_INFO_KEY-1)
	{
		Com_Printf ("Keys and values must be < 64 characters.\n");
		return;
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	Com_sprintf (newi, sizeof(newi), "\\%s\\%s", key, value);

#if 0
	if (strlen(newi) + strlen(s) > maxsize)
	{
		Com_Printf ("Info string length exceeded\n");
		return;
	}
#endif

	strcat (newi, s);
	strcpy (s, newi);

#if 0
	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;		// strip high bits
		if (c >= 32 && c < 127)
			*s++ = c;
	}
	*s = 0;
#endif
}


/*
==============================================================
INVENTORY MANAGEMENT
==============================================================
*/

csi_t		*CSI;
invList_t	*invUnused;
item_t		cacheItem;
invList_t	cacheList;

/*
=================
Com_InitCSI
=================
*/
void Com_InitCSI( csi_t *import )
{
	CSI = import;
}

/*
=================
Com_InitInventory
=================
*/
void Com_InitInventory( invList_t *invList )
{
	invList_t	*last;
	int	i;
	assert( invList );

	invUnused = invList;
	invUnused->next = NULL;
	for ( i = 0; i < MAX_INVLIST-1; i++ )
	{
		last = invUnused++;
		invUnused->next = last;
	}
}

/*
=================
Com_CheckToInventory
=================
*/
qboolean Com_CheckToInventory( inventory_t *i, int item, int container, int x, int y )
{
	invList_t	*ic, *right, *left;
	int		mask[16];
	int		j;

	assert( i );
	// armor vs item
	if ( !strcmp( CSI->ods[item].type, "armor" ) )
	{
		if ( !CSI->ids[container].armor && !CSI->ids[container].all ) return false;
	}
	else if ( CSI->ids[container].armor ) return false;

	// special hand checks
	right = i->c[CSI->idRight];
	left  = i->c[CSI->idLeft];

	if ( container == CSI->idRight )
	{
		if ( !right && ( !CSI->ods[item].twohanded || !left ) ) return true;
		else return false;
	}
	else if ( container == CSI->idLeft )
	{
		if ( !left && ((right && !CSI->ods[right->item.t].twohanded
			&& !CSI->ods[item].twohanded) || !right) ) return true;
		else return false;
	}

	// single item containers
	if ( CSI->ids[container].single )
	{
		if ( i->c[container] ) return false;
		else return true;
	}

	// check bounds
	if ( x < 0 || y < 0 || x >= 32 || y >= 16 )
		return false;

	// extract shape info
	for ( j = 0; j < 16; j++ )
		mask[j] = ~CSI->ids[container].shape[j];

	// add other items to mask
	for ( ic = i->c[container]; ic; ic = ic->next )
		for ( j = 0; j < 4 && ic->y+j < 16; j++ )
			mask[ic->y+j] |= ((CSI->ods[ic->item.t].shape >> j*8) & 0xFF) << ic->x;

	// test for collisions with newly generated mask
	for ( j = 0; j < 4; j++ )
		if ( (((CSI->ods[item].shape >> j*8) & 0xFF) << x) & mask[y+j] )
			return false;

	// everything ok
	return true;
}


/*
=================
Com_SearchInInventory
=================
*/
invList_t *Com_SearchInInventory( inventory_t *i, int container, int x, int y )
{
	invList_t	*ic;

	if ( CSI->ids[container].single )
		return i->c[container];

	for ( ic = i->c[container]; ic; ic = ic->next )
		if ( x >= ic->x && y >= ic->y && x < ic->x+8 && y < ic->y+4 &&
			((CSI->ods[ic->item.t].shape >> (x-ic->x) >> (y-ic->y)*8)) & 1 )
			return ic;

	// found nothing
	return NULL;
}


/*
=================
Com_AddToInventory
=================
*/
invList_t *Com_AddToInventory( inventory_t *i, item_t item, int container, int x, int y )
{
	invList_t	*ic;

	if ( !invUnused )
	{
		Com_Printf( _("No free inventory space!\n") );
		return NULL;
	}
	if ( item.t == NONE ) return NULL;

	assert( i );
	// allocate space
	ic = i->c[container];
	i->c[container] = invUnused;
	invUnused = invUnused->next;
	i->c[container]->next = ic;
	ic = i->c[container];

	// set the data
	ic->item = item;
	ic->x = x;
	ic->y = y;
	return ic;
}

/*
=================
Com_RemoveFromInventory
=================
*/
qboolean Com_RemoveFromInventory( inventory_t *i, int container, int x, int y )
{
	invList_t	*ic, *old;

	assert( i );
	ic = i->c[container];
	if ( !ic ) return false;

	if ( CSI->ids[container].single || ( ic->x == x && ic->y == y ) )
	{
		old = invUnused;
		invUnused = ic;
		cacheItem = ic->item;
		i->c[container] = ic->next;
		invUnused->next = old;
		return true;
	}

	for ( ; ic->next; ic = ic->next )
		if ( ic->next->x == x && ic->next->y == y )
		{
			old = invUnused;
			invUnused = ic->next;
			cacheItem = ic->next->item;
			ic->next = ic->next->next;
			invUnused->next = old;
			return true;
		}

	return false;
}

/*
=================
Com_MoveInInventory
=================
*/
int Com_MoveInInventory( inventory_t *i, int from, int fx, int fy, int to, int tx, int ty, int *TU, invList_t **icp )
{
	invList_t	*ic;
	int		time;
 	item_t cacheItem2;

	if ( icp ) *icp = NULL;

	if ( from == to && fx == tx && fy == ty )
		return 0;

	time = CSI->ids[from].out + CSI->ids[to].in;
	if ( from == to ) time /= 2;
	if ( TU && *TU < time )
		return IA_NOTIME;

	assert( i );

	if ( !Com_RemoveFromInventory( i, from, fx, fy ) ) return IA_NONE; // break if source item is not removeable

	// if weapon is twohanded and is moved from hand to hand do nothing.
	if ( CSI->ods[cacheItem.t].twohanded
	&& ( ( to == CSI->idLeft && from == CSI->idRight ) || ( to == CSI->idRight && from == CSI->idLeft ) ) )
		return IA_NONE;

	//check if the target is a blocked inv-armor and source!=dest
 	if ( CSI->ids[to].armor && !Com_CheckToInventory( i, cacheItem.t, to, tx, ty ) && from!=to ) {
 		cacheItem2 = cacheItem;						// save/chache (source) item
 		Com_MoveInInventory( i, to, tx, ty, from, fx, fy, TU, icp );	// move the destination item to the source
 		cacheItem = cacheItem2;						// reset the cached item (source)
 	}
	if ( !Com_CheckToInventory( i, cacheItem.t, to, tx, ty ) )
	{
		ic = Com_SearchInInventory( i, to, tx, ty );

		if ( ic && CSI->ods[cacheItem.t].link == ic->item.t )
		{
			if ( ic->item.a >= CSI->ods[ic->item.t].ammo )
			{
				// weapon already loaded
				Com_AddToInventory( i, cacheItem, from, fx, fy );
				return IA_NORELOAD;
			}

			time += CSI->ods[ic->item.t].reload;
			if ( !TU || *TU >= time )
			{
				if ( TU ) *TU -= time;
				ic->item.a = CSI->ods[ic->item.t].ammo;
				ic->item.m = cacheItem.t;
				if ( icp ) *icp = ic;
				return IA_RELOAD;
			}
			// not enough time
			Com_AddToInventory( i, cacheItem, from, fx, fy );
			return IA_NOTIME;
		}

		// impossible move
		Com_AddToInventory( i, cacheItem, from, fx, fy );
		return IA_NONE;
	}

	// twohanded exception
	if ( CSI->ods[cacheItem.t].twohanded && to == CSI->idLeft )
		to = CSI->idRight;

	// successful
	if ( TU ) *TU -= time;
	ic = Com_AddToInventory( i, cacheItem, to, tx, ty );

	// return data
	if ( icp ) *icp = ic;
	if ( to == CSI->idArmor ) return IA_ARMOR;
	else return IA_MOVE;
}

/*
=================
Com_EmptyContainer
=================
*/
void Com_EmptyContainer( inventory_t *i, int container )
{
	invList_t	*ic, *old;
	int cnt = 0;

	assert( i );
	ic = i->c[container];
	while ( ic )
	{
		old = ic;
		ic = ic->next;
		old->next = invUnused;
		invUnused = old;
		if ( cnt >= MAX_INVLIST )
		{
			Com_Printf("Error: There are more than the allowed entries (Com_EmptyContainer)\n");
			break;
		}
		cnt++;
	}
	i->c[container] = NULL;
}

/*
=================
Com_DestroyInventory
=================
*/
void Com_DestroyInventory( inventory_t *i )
{
	int k;

	if ( !i ) return;

	for ( k = 0; k < CSI->numIDs; k++ )
		Com_EmptyContainer( i, k );
}

/*
=================
Com_FindSpace
=================
*/
void Com_FindSpace( inventory_t *inv, int item, int container, int *px, int *py )
{
	int	x, y;
	assert( inv );

	for ( y = 0; y < 16; y++ )
		for ( x = 0; x < 32; x++ )
			if ( Com_CheckToInventory( inv, item, container, x, y ) )
			{
				*px = x; *py = y;
				return;
			}

	*px = *py = NONE;
}

/*
==============================================================================
CHARACTER GENERATION AND HANDLING
==============================================================================
*/

/*
======================
Com_CharGenAbilitySkills
======================
*/
#define MAX_GENCHARRETRIES	20

void Com_CharGenAbilitySkills( character_t *chr, int minAbility, int maxAbility, int minSkill, int maxSkill )
{
	float randomArray[SKILL_NUM_TYPES];
	int total;
	int i, retry;
	float max, min, rand_avg;

	assert( chr );
	retry = MAX_GENCHARRETRIES;
	do {
		// Abilities
		total = ABILITY_NUM_TYPES*(maxAbility-minAbility)/2;
		max = 0;
		min = 1;
		rand_avg = 0;
		for (i = 0; i < ABILITY_NUM_TYPES; i++)
		{
			randomArray[i] = frand();
			rand_avg += randomArray[i];
			if (randomArray[i] > max)
				max = randomArray[i]; // get max
			if (randomArray[i] < min)
				min = randomArray[i]; // and min
		}

		rand_avg /= ABILITY_NUM_TYPES;
		if (max-rand_avg < rand_avg-min) min = rand_avg-min;
		else min = max-rand_avg;
		for (i = 0; i < ABILITY_NUM_TYPES; i++)
			chr->skills[i] = ((randomArray[i]-rand_avg)/min*(maxAbility-minAbility) +
				minAbility+maxAbility)/2 + frand()*3;

		// Skills
		total = (SKILL_NUM_TYPES-ABILITY_NUM_TYPES)*(maxSkill-minSkill)/2;
		max = 0;
		min = 1;
		rand_avg = 0;
		for (i = 0; i < SKILL_NUM_TYPES-ABILITY_NUM_TYPES; i++)
		{
			randomArray[i] = frand();
			rand_avg += randomArray[i];
			if (randomArray[i] > max)
				max = randomArray[i]; // get max
			if (randomArray[i] < min)
				min = randomArray[i]; // or min
		}
		rand_avg /= SKILL_NUM_TYPES-ABILITY_NUM_TYPES;
		if (max-rand_avg < rand_avg-min) min= rand_avg-min;
		else min = max-rand_avg;
		for (i = 0; i < SKILL_NUM_TYPES-ABILITY_NUM_TYPES; i++)
			chr->skills[ABILITY_NUM_TYPES+i] = ((randomArray[i]-rand_avg)/min*(maxSkill-minSkill) +
				minSkill+maxSkill)/2 + frand()*3;

		// Check if it makes sense
		max = ((max-rand_avg)/min*(maxSkill-minSkill) + minSkill+maxSkill)/2;
		min = (maxAbility+minAbility)/2.2;
		if (
		    (max-10 < chr->skills[SKILL_CLOSE]     && (chr->skills[ABILITY_SPEED]   < min || chr->skills[ABILITY_POWER] < min))
		 || (max-10 < chr->skills[SKILL_HEAVY]     && chr->skills[ABILITY_POWER]    < min)
		 || (max-10 < chr->skills[SKILL_PRECISE]   && chr->skills[ABILITY_ACCURACY] < min)
		 || (max-10 < chr->skills[SKILL_EXPLOSIVE] && chr->skills[ABILITY_MIND]     < min)
		) retry--; // try again.
		else retry = 0;
	}
	while ( retry > 0 );
}


char returnModel[MAX_VAR];

/*
======================
Com_CharGetBody
======================
*/
char *Com_CharGetBody( character_t *chr )
{
	assert( chr );
	if ( chr->inv->c[CSI->idArmor] )
		sprintf( returnModel, "%s%s/%s", chr->path, CSI->ods[chr->inv->c[CSI->idArmor]->item.t].kurz, chr->body );
	else
		sprintf( returnModel, "%s/%s", chr->path, chr->body );
	return returnModel;
}


/*
======================
Com_CharGetHead
======================
*/
char *Com_CharGetHead( character_t *chr )
{
	assert( chr );
	if ( chr->inv->c[CSI->idArmor] )
		sprintf( returnModel, "%s%s/%s", chr->path, CSI->ods[chr->inv->c[CSI->idArmor]->item.t].kurz, chr->head );
	else
		sprintf( returnModel, "%s/%s", chr->path, chr->head );
	return returnModel;
}


/*
==============================================================================
SCRIPT VALUE PARSING
==============================================================================
*/

char *vt_names[V_NUM_TYPES] =
{
	"",
	"bool",
	"char",
	"int",
	"float",
	"pos",
	"vector",
	"color",
	"rgba",
	"string",
	"longstring",
	"pointer",
	"align",
	"blend",
	"style",
	"fade",
	"shapes",
	"shapeb",
	"dmgtype",
	"date"
};

char *align_names[ALIGN_LAST] =
{
	"ul", "uc", "ur", "cl", "cc", "cr", "ll", "lc", "lr"
};

char *blend_names[BLEND_LAST] =
{
	"replace", "blend", "add", "filter", "invfilter"
};

char *style_names[STYLE_LAST] =
{
	"facing", "rotated", "beam", "line"
};

char *fade_names[FADE_LAST] =
{
	"none", "in", "out", "sin", "saw", "blend"
};

// this is here to let Com_ParseValue determine the right size
typedef struct menuDepends_s
{
	char var[MAX_VAR];
	char value[MAX_VAR];
} menuDepends_t;

/*
=================
Com_ParseValue

translateable string are marked with _ at the beginning
example:
menu example
{
 string "_this is translatable"
}
=================
*/
int Com_ParseValue( void *base, char *token, int type, int ofs )
{
	byte	*b;
	char	string[MAX_VAR];
	char	string2[MAX_VAR];
	int		x, y, w, h;

	b = (byte *)base + ofs;

	switch (type)
	{
	case V_NULL:
		return 0;

	case V_BOOL:
		if ( !strcmp( token, "true" ) || !strcmp( token, "1" ) )
			*b = true;
		else
			*b = false;
		return sizeof(byte);

	case V_CHAR:
		*(char *)b = *token;
		return sizeof(char);

	case V_INT:
		*(int *)b = atoi(token);
		return sizeof(int);

	case V_FLOAT:
		*(float *)b = atof(token);
		return sizeof(float);

	case V_POS:
		if ( strstr(token, " ") == NULL )
			Sys_Error( _("Com_ParseValue: Illegal pos statement\n") );
		sscanf( token, "%f %f",
			&((float *)b)[0], &((float *)b)[1] );
		return 2*sizeof(float);

	case V_VECTOR:
		if ( strstr( strstr(token, " "), " ") == NULL )
			Sys_Error( _("Com_ParseValue: Illegal vector statement\n") );
		sscanf( token, "%f %f %f",
			&((float *)b)[0], &((float *)b)[1], &((float *)b)[2] );
		return 3*sizeof(float);

	case V_COLOR:
		if ( strstr( strstr( strstr(token, " "), " "), " " ) == NULL )
			Sys_Error( _("Com_ParseValue: Illegal color statement\n") );
		sscanf( token, "%f %f %f %f",
			&((float *)b)[0], &((float *)b)[1],
			&((float *)b)[2], &((float *)b)[3] );
		return 4*sizeof(float);

	case V_RGBA:
		if ( strstr( strstr( strstr(token, " "), " "), " " ) == NULL )
			Sys_Error( _("Com_ParseValue: Illegal rgba statement\n") );
		sscanf( token, "%i %i %i %i",
			&((int *)b)[0], &((int *)b)[1],
			&((int *)b)[2], &((int *)b)[3] );
		return 4;

	case V_STRING:
		// i18n marked through _ at beginning of string
		if ( *token == '_' )
			token++;
		strncpy( (char *)b, token, MAX_VAR );
		w = strlen(token)+1;
		if ( w > MAX_VAR ) w = MAX_VAR;
		return w;

	case V_LONGSTRING:
		strcpy( (char *)b, token );
		w = strlen(token)+1;
		return w;

	case V_POINTER:
		*(void **)b = (void *)token;
		return sizeof(void*);

	case V_ALIGN:
		for ( w = 0; w < ALIGN_LAST; w++ )
			if ( !strcmp( token, align_names[w] ) )
				break;
		if ( w == ALIGN_LAST ) *b = 0;
		else *b = w;
		return 1;

	case V_BLEND:
		for ( w = 0; w < BLEND_LAST; w++ )
			if ( !strcmp( token, blend_names[w] ) )
				break;
		if ( w == BLEND_LAST ) *b = 0;
		else *b = w;
		return 1;

	case V_STYLE:
		for ( w = 0; w < STYLE_LAST; w++ )
			if ( !strcmp( token, style_names[w] ) )
				break;
		if ( w == STYLE_LAST ) *b = 0;
		else *b = w;
		return 1;

	case V_FADE:
		for ( w = 0; w < FADE_LAST; w++ )
			if ( !strcmp( token, fade_names[w] ) )
				break;
		if ( w == FADE_LAST ) *b = 0;
		else *b = w;
		return 1;

	case V_SHAPE_SMALL:
		if ( strstr( strstr( strstr(token, " "), " "), " " ) == NULL )
			Sys_Error( _("Com_ParseValue: Illegal shape small statement\n") );
		sscanf( token, "%i %i %i %i", &x, &y, &w, &h );
		for ( h += y; y < h ; y++ )
			*(int *)b |= ((1<<w)-1) << x << (y*8);
		return 4;

	case V_SHAPE_BIG:
		if ( strstr( strstr( strstr(token, " "), " "), " " ) == NULL )
			Sys_Error( _("Com_ParseValue: Illegal shape big statement\n") );
		sscanf( token, "%i %i %i %i", &x, &y, &w, &h );
		w = ((1<<w)-1) << x;
		for ( h += y; y < h ; y++ )
			((int *)b)[y] |= w;
		return 64;

	case V_DMGTYPE:
		for ( w = 0; w < CSI->numDTs; w++ )
			if ( !strcmp( token, CSI->dts[w] ) )
				break;
		if ( w == CSI->numDTs ) *b = 0;
		else *b = w;
		return 1;

	case V_DATE:
		if ( strstr( strstr(token, " "), " ") == NULL )
			Sys_Error( _("Com_ParseValue: Illegal if statement\n") );
		sscanf( token, "%i %i %i", &x, &y, &w );
		((date_t*)b)->day = 365*x + y;
		((date_t*)b)->sec = 3600*w;
		return sizeof(date_t);

	case V_IF:
		if ( strstr(token, " ") == NULL )
			Sys_Error( _("Com_ParseValue: Illegal if statement\n") );
		sscanf( token, "%s %s", string, string2 );
		strncpy ( ((menuDepends_t*)b)->var, string, MAX_VAR );
		strncpy ( ((menuDepends_t*)b)->value, string2, MAX_VAR );
		return sizeof(menuDepends_t);

	default:
		Sys_Error( _("Com_ParseValue: unknown value type\n") );
		return -1;
	}
}



/*
=================
Com_SetValue
=================
*/
int Com_SetValue( void *base, void *set, int type, int ofs )
{
	byte	*b;
	int		len;

	b = (byte *)base + ofs;

	switch (type)
	{
	case V_NULL:
		return 0;

	case V_BOOL:
		if ( *(byte *)set )
			*b = true;
		else
			*b = false;
		return sizeof(byte);

	case V_CHAR:
		*(char *)b = *(char *)set;
		return sizeof(char);

	case V_INT:
		*(int *)b = *(int *)set;
		return sizeof(int);

	case V_FLOAT:
		*(float *)b = *(float *)set;
		return sizeof(float);

	case V_POS:
		((float *)b)[0] = ((float *)set)[0];
		((float *)b)[1] = ((float *)set)[1];
		return 2*sizeof(float);

	case V_VECTOR:
		((float *)b)[0] = ((float *)set)[0];
		((float *)b)[1] = ((float *)set)[1];
		((float *)b)[2] = ((float *)set)[2];
		return 3*sizeof(float);

	case V_COLOR:
		((float *)b)[0] = ((float *)set)[0];
		((float *)b)[1] = ((float *)set)[1];
		((float *)b)[2] = ((float *)set)[2];
		((float *)b)[3] = ((float *)set)[3];
		return 4*sizeof(float);

	case V_RGBA:
		((byte *)b)[0] = ((byte *)set)[0];
		((byte *)b)[1] = ((byte *)set)[1];
		((byte *)b)[2] = ((byte *)set)[2];
		((byte *)b)[3] = ((byte *)set)[3];
		return 4;

	case V_STRING:
		strncpy( (char *)b, (char *)set, MAX_VAR );
		len = strlen((char *)set)+1;
		if ( len > MAX_VAR ) len = MAX_VAR;
		return len;

	case V_LONGSTRING:
		strcpy( (char *)b, (char *)set );
		len = strlen((char *)set)+1;
		return len;

	case V_ALIGN:
	case V_BLEND:
	case V_STYLE:
	case V_FADE:
		*b = *(byte *)set;
		return 1;

	case V_SHAPE_SMALL:
		*(int *)b = *(int *)set;
		return 4;

	case V_SHAPE_BIG:
		memcpy( b, set, 64 );
		return 64;

	case V_DMGTYPE:
		*b = *(byte *)set;
		return 1;

	case V_DATE:
		memcpy( b, set, sizeof(date_t) );
		return sizeof(date_t);

	default:
		Sys_Error( "Com_ParseValue: unknown value type\n" );
		return -1;
	}
}

/*
=================
Com_ValueToStr
=================
*/
char valuestr[MAX_VAR];

char *Com_ValueToStr( void *base, int type, int ofs )
{
	byte	*b;

	b = (byte *)base + ofs;

	switch (type)
	{
	case V_NULL:
		return 0;

	case V_BOOL:
		if ( *b ) return "true";
		else return "false";

	case V_CHAR:
		return (char *)b;
		break;

	case V_INT:
		sprintf( valuestr, "%i", *(int *)b );
		return valuestr;

	case V_FLOAT:
		sprintf( valuestr, "%f", *(float *)b );
		return valuestr;

	case V_POS:
		sprintf( valuestr, "%.2f %.2f",
			((float *)b)[0], ((float *)b)[1] );
		return valuestr;

	case V_VECTOR:
		sprintf( valuestr, "%.2f %.2f %.2f",
			((float *)b)[0], ((float *)b)[1], ((float *)b)[2] );
		return valuestr;

	case V_COLOR:
		sprintf( valuestr, "%.2f %.2f %.2f %.2f",
			((float *)b)[0], ((float *)b)[1],
			((float *)b)[2], ((float *)b)[3] );
		return valuestr;

	case V_RGBA:
		sprintf( valuestr, "%3i %3i %3i %3i",
			((byte *)b)[0], ((byte *)b)[1],
			((byte *)b)[2], ((byte *)b)[3] );
		return valuestr;

	case V_STRING:
	case V_LONGSTRING:
		return (char *)b;

	case V_ALIGN:
		return align_names[*b];

	case V_BLEND:
		return blend_names[*b];

	case V_STYLE:
		return style_names[*b];

	case V_FADE:
		return fade_names[*b];

	case V_SHAPE_SMALL:
	case V_SHAPE_BIG:
		return "";

	case V_DMGTYPE:
		return CSI->dts[*b];

	case V_DATE:
		sprintf( valuestr, "%i %i %i",
			((date_t*)b)->day / 365, ((date_t*)b)->day % 365, ((date_t*)b)->sec );
		return valuestr;

	default:
		Sys_Error( "Com_ParseValue: unknown value type\n" );
		return NULL;
	}
}

void Com_InventoryList_f ( void )
{
	int i;
	objDef_t* ods_temp;
	for (i=0; i < CSI->numODs; i++) {
		ods_temp = &CSI->ods[i];
		Com_Printf( _("Item: %s\n"), ods_temp->kurz );
		Com_Printf( _("... name      -> %s\n"), ods_temp->name );
		Com_Printf( _("... type      -> %s\n"), ods_temp->type );
		Com_Printf( _("... category  -> %i\n"), ods_temp->category );
		Com_Printf( _("... weapon    -> %i\n"), ods_temp->weapon );
		Com_Printf( _("... twohanded -> %i\n"), ods_temp->twohanded );
		Com_Printf( _("... thrown    -> %i\n"), ods_temp->thrown );
	}
}
