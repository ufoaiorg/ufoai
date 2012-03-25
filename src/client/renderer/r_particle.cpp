/**
 * @file r_main.c
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "r_draw.h"

ptl_t r_particleArray[MAX_PTLS];
int r_numParticles;

cvar_t *r_particles;

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
	{
		/* draw it */
		const vec3_t points[] = { { pos[0], pos[1], pos[2] }, { pos[0] + up[0], pos[1] + up[1], pos[2] + up[2] }, { pos[0]
				+ up[0] + right[0], pos[1] + up[1] + right[1], pos[2] + up[2] + right[2] }, { pos[0] + right[0], pos[1]
				+ right[1], pos[2] + right[2] } };

		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, texcoords);
		R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, points);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		R_BindDefaultArray(GL_VERTEX_ARRAY);
		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

		refdef.batchCount++;
	}
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
	const float thickness = p->size[1];

	R_EnableTexture(&texunit_diffuse, qfalse);

	R_DrawCircle(radius, p->color, thickness, p->s);

	R_EnableTexture(&texunit_diffuse, qtrue);
}

/**
 * @sa R_DrawParticles
 */
static void R_DrawPtlLine (const ptl_t * p)
{
	const vec3_t points[] = { { p->s[0], p->s[1], p->s[2] }, { p->v[0], p->v[1], p->v[2] } };

	R_EnableTexture(&texunit_diffuse, qfalse);

	glEnable(GL_LINE_SMOOTH);

	R_Color(p->color);

	/* draw line from s to v */
	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, points);
	glDrawArrays(GL_LINE_STRIP, 0, 2);
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	refdef.batchCount++;

	R_Color(NULL);

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

	if (!r_particles->integer)
		return;

	for (i = 0, p = r_particleArray; i < r_numParticles; i++, p++)
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
