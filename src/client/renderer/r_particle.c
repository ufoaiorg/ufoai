/**
 * @file r_main.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "r_local.h"
#include "r_particle.h"

ptlArt_t r_particlesArt[MAX_PTL_ART];
int r_numParticlesArt;

ptl_t r_particles[MAX_PTLS];
int r_numParticles;

/*
=============================================================
PARTICLE DRAWING
=============================================================
*/

/**
 * @brief Get Sprite Vectors
 * @param[in] p the particle to give the dimensions for
 * @param[out] right the output right vector for the particle
 * @param[out] up the output up vector for the particle
 * @sa R_DrawSprite
 */
static void R_GetSpriteVectors (const ptl_t *p, vec3_t right, vec3_t up)
{
	/* get transformation */
	switch (p->style) {
	case STYLE_FACING:
		VectorScale(r_locals.right, p->size[0], right);
		VectorScale(r_locals.up, p->size[1], up);
		break;

	case STYLE_ROTATED:
		AngleVectors(p->angles, NULL, right, up);
		VectorScale(right, p->size[0], right);
		VectorScale(up, p->size[1], up);
		break;

	case STYLE_BEAM:
	case STYLE_AXIS:
		AngleVectors(p->angles, right, NULL, NULL);
		CrossProduct(right, r_locals.forward, up);
		VectorNormalizeFast(up);
		VectorScale(right, p->size[0], right);
		VectorScale(up, p->size[1], up);
		break;

	default:
		Com_Error(ERR_FATAL, "R_GetSpriteVectors: unknown style");
	}
}

/**
 * @brief Fills float array with texture coordinates
 * @note Used if sprite is scrolled
 */
static inline void R_SpriteTexcoords (const ptl_t *p, float out[8])
{
	const float s = p->scrollS * refdef.time;
	const float t = p->scrollT * refdef.time;

	out[0] = 0.0 + s;
	out[1] = 0.0 + t;

	out[2] = 0.0 + s;
	out[3] = 1.0 + t;

	out[4] = 1.0 + s;
	out[5] = 1.0 + t;

	out[6] = 1.0 + s;
	out[7] = 0.0 + t;
}

/**
 * @sa R_DrawParticles
 */
static void R_DrawSprite (const ptl_t * p)
{
	const ptl_t *q;
	vec3_t up, right;
	vec3_t nup, nright;
	vec3_t pos;
	float texcoords[8];

	/* load texture set up coordinates */
	assert(p->pic);
	R_BindTexture(p->pic->art.image->texnum);

	/* calculate main position and normalised up and right vectors */
	q = p->parent ? p->parent : p;
	R_GetSpriteVectors(q, right, up);

	/* Calculate normalised */
	VectorCopy(up, nup);
	VectorCopy(right, nright);
	VectorNormalizeFast(nup);
	VectorNormalizeFast(nright);

	/* offset */
	VectorCopy(q->s, pos);
	VectorMA(pos, q->offset[0], nup, pos);
	VectorMA(pos, q->offset[1], nright, pos);

	if (p->parent) {
		/* if this is a child then calculate our own up and right vectors and offsets */
		R_GetSpriteVectors(p, right, up);

		/* but offset by our parent's nup and nright */
		VectorMA(pos, p->offset[0], nup, pos);
		VectorMA(pos, p->offset[1], nright, pos);
	}

	/* center image */
	VectorMA(pos, -0.5, up, pos);
	VectorMA(pos, -0.5, right, pos);

	R_SpriteTexcoords(p, texcoords);

	R_Color(p->color);
	/* draw it */
#ifdef GL_VERSION_ES_CM_1_0
	GLfloat points[3*4] = {	pos[0], pos[1], pos[2],
							pos[0] + up[0], pos[1] + up[1], pos[2] + up[2],
							pos[0] + up[0] + right[0], pos[1] + up[1] + right[1], pos[2] + up[2] + right[2],
							pos[0] + right[0], pos[1] + right[1], pos[2] + right[2] };
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, texcoords);
	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, points);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
#else
	glBegin(GL_TRIANGLE_FAN);

	glTexCoord2f(texcoords[0], texcoords[1]);
	glVertex3fv(pos);

	VectorAdd(pos, up, pos);
	glTexCoord2f(texcoords[2], texcoords[3]);
	glVertex3fv(pos);

	VectorAdd(pos, right, pos);
	glTexCoord2f(texcoords[4], texcoords[5]);
	glVertex3fv(pos);

	VectorSubtract(pos, up, pos);
	glTexCoord2f(texcoords[6], texcoords[7]);
	glVertex3fv(pos);

	glEnd();
#endif
}


/**
 * @sa R_DrawParticles
 */
static void R_DrawParticleModel (ptl_t * p)
{
	modelInfo_t mi;

	/* initialize model info */
	OBJZERO(mi);
	mi.color = p->color;
	mi.origin = p->s;
	mi.angles = p->angles;
	assert(p->model);
	mi.model = p->model->art.model;
	mi.skin = p->skin;

	/* draw it */
	R_DrawModelParticle(&mi);
}

/**
 * @brief Draws a circle out of lines
 * @param[in] p The particle definition with origin, radius and color
 * @sa Draw_Circle
 */
static void R_DrawPtlCircle (const ptl_t* p)
{
	const float radius = p->size[0];
	const int thickness = (int)p->size[1];
	float theta;
	const float accuracy = 5.0f;

	R_EnableTexture(&texunit_diffuse, qfalse);

	R_Color(p->color);

	glEnable(GL_LINE_SMOOTH);

	assert(radius > thickness);

#ifdef GL_VERSION_ES_CM_1_0
	// TODO: no thickness
	enum { STEPS = 16 };
	GLfloat points [ STEPS * 3 ];
	for (int i = 0; i < STEPS; i++ ) {
		float a = 2.0f * M_PI * (float) i / (float) STEPS;
		points[i*3] = p->s[0] + radius * cos( a );
		points[i*3] = p->s[1] + radius * sin( a );
		points[i*3] = p->s[2];
	}
	glVertexPointer(3, GL_FLOAT, 0, points);
	glDrawArrays(GL_LINE_LOOP, 0, STEPS);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
#else
	if (thickness <= 1) {
		glBegin(GL_LINE_LOOP);
		for (theta = 0.0f; theta < 2.0f * M_PI; theta += M_PI / (radius * accuracy)) {
			glVertex3f(p->s[0] + radius * cos(theta), p->s[1] + radius * sin(theta), p->s[2]);
		}
		glEnd();
	} else {
		const float delta = M_PI / (radius * accuracy);
		glBegin(GL_TRIANGLE_STRIP);
		for (theta = 0; theta <= 2 * M_PI; theta += delta) {
			const float f = theta - M_PI / (radius * accuracy);
			glVertex3f(p->s[0] + radius * cos(theta), p->s[1] + radius * sin(theta), p->s[2]);
			glVertex3f(p->s[0] + radius * cos(f), p->s[1] + radius * sin(f), p->s[2]);
			glVertex3f(p->s[0] + (radius - thickness) * cos(f), p->s[1] + (radius - thickness) * sin(f), p->s[2]);
			glVertex3f(p->s[0] + (radius - thickness) * cos(theta), p->s[1] + (radius - thickness) * sin(theta), p->s[2]);
		}
		glEnd();
	}
#endif

	glDisable(GL_LINE_SMOOTH);

	R_EnableTexture(&texunit_diffuse, qtrue);
}

/**
 * @sa R_DrawParticles
 */
static void R_DrawPtlLine (const ptl_t * p)
{
	R_EnableTexture(&texunit_diffuse, qfalse);

	glEnable(GL_LINE_SMOOTH);

	R_Color(p->color);

	/* draw line from s to v */
#ifdef GL_VERSION_ES_CM_1_0
	GLfloat points [ 6 ] = { p->s[0], p->s[1], p->s[2], p->v[0], p->v[1], p->v[2] };
	glVertexPointer(3, GL_FLOAT, 0, points);
	glDrawArrays(GL_LINE_STRIP, 0, 2);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
#else
	glBegin(GL_LINE_STRIP);
	glVertex3fv(p->s);
	glVertex3fv(p->v);
	glEnd();
#endif

	glDisable(GL_LINE_SMOOTH);

	R_EnableTexture(&texunit_diffuse, qtrue);
}


/*
=============================================================
GENERIC PARTICLE FUNCTIONS
=============================================================
*/

/**
 * @sa R_DrawParticles
 */
static void R_SetBlendMode (int mode)
{
	switch (mode) {
	case BLEND_REPLACE:
		R_TexEnv(GL_REPLACE);
		break;
	case BLEND_ONE:
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		break;
	case BLEND_BLEND:
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case BLEND_ADD:
		R_BlendFunc(GL_ONE, GL_ONE);
		break;
	case BLEND_FILTER:
		R_BlendFunc(GL_ZERO, GL_SRC_COLOR);
		break;
	case BLEND_INVFILTER:
		R_BlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		break;
	default:
		Com_Error(ERR_DROP, "unknown blend mode");
		break;
	}
}

/**
 * @note No need to reset the blend mode - R_Setup2D will do this
 * @sa R_Setup2D
 */
void R_DrawParticles (void)
{
	ptl_t *p;
	int i;

	for (i = 0, p = r_particles; i < r_numParticles; i++, p++)
		if (p->inuse && !p->invis) {
			/* test for visibility */
			if (p->levelFlags && !((1 << refdef.worldlevel) & p->levelFlags))
				continue;

			if (p->program != NULL)
				R_UseProgram(p->program);

			/* set blend mode and draw gfx */
			R_SetBlendMode(p->blend);
			switch (p->style) {
			case STYLE_LINE:
				R_DrawPtlLine(p);
				break;
			case STYLE_CIRCLE:
				R_DrawPtlCircle(p);
				break;
			default:
				break;
			}
			if (p->pic)
				R_DrawSprite(p);
			if (p->model)
				R_DrawParticleModel(p);
			R_TexEnv(GL_MODULATE);

			R_UseProgram(NULL);
		}

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_Color(NULL);
}
