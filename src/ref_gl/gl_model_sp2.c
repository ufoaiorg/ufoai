/**
 * @file gl_model_sp2.c
 * @brief sprite model loading
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

#include "gl_local.h"

/*
==============================================================================
SPRITE MODELS
==============================================================================
*/


/**
 * @brief
 */
void R_DrawSpriteModel (entity_t * e)
{
	float alpha = 1.0F;
	vec3_t point;
	dsprframe_t *frame;
	float *up, *right;
	dsprite_t *psprite;

	/* don't even bother culling, because it's just a single */
	/* polygon without a surface cache */
	assert(currentmodel->type == mod_sprite);
	psprite = (dsprite_t *) currentmodel->alias.extraData;

#if 0
	if (e->frame < 0 || e->frame >= psprite->numframes) {
		ri.Con_Printf(PRINT_ALL, "no such sprite frame %i\n", e->frame);
		e->frame = 0;
	}
#endif
	e->as.frame %= psprite->numframes;

	frame = &psprite->frames[e->as.frame];

#if 0
	if (psprite->type == SPR_ORIENTED) {	/* bullet marks on walls */
		vec3_t v_forward, v_right, v_up;

		AngleVectors(currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	} else
#endif
	{							/* normal sprite */
		up = vup;
		right = vright;
	}

	if (e->flags & RF_TRANSLUCENT)
		alpha = e->alpha;

	if (alpha != 1.0F)
		GLSTATE_ENABLE_BLEND

	qglColor4f(1, 1, 1, alpha);

	GL_Bind(currentmodel->alias.skins_img[e->as.frame]->texnum);

	GL_TexEnv(GL_MODULATE);

	if (alpha == 1.0) {
		GLSTATE_ENABLE_ALPHATEST
	} else {
		GLSTATE_DISABLE_ALPHATEST
	}

	qglBegin(GL_QUADS);

	qglTexCoord2f(0, 1);
	VectorMA(e->origin, -frame->origin_y, up, point);
	VectorMA(point, -frame->origin_x, right, point);
	qglVertex3fv(point);

	qglTexCoord2f(0, 0);
	VectorMA(e->origin, frame->height - frame->origin_y, up, point);
	VectorMA(point, -frame->origin_x, right, point);
	qglVertex3fv(point);

	qglTexCoord2f(1, 0);
	VectorMA(e->origin, frame->height - frame->origin_y, up, point);
	VectorMA(point, frame->width - frame->origin_x, right, point);
	qglVertex3fv(point);

	qglTexCoord2f(1, 1);
	VectorMA(e->origin, -frame->origin_y, up, point);
	VectorMA(point, frame->width - frame->origin_x, right, point);
	qglVertex3fv(point);

	qglEnd();

	GLSTATE_DISABLE_ALPHATEST
	GL_TexEnv(GL_REPLACE);

	if (alpha != 1.0F)
		GLSTATE_DISABLE_BLEND

	qglColor4f(1, 1, 1, 1);
}

/**
 * @brief
 */
void Mod_LoadSpriteModel (model_t * mod, void *buffer, int bufSize)
{
	dsprite_t *sprin, *sprout;
	int i;

	sprin = (dsprite_t *) buffer;
	sprout = ri.TagMalloc(ri.modelPool, bufSize, 0);

	sprout->ident = LittleLong(sprin->ident);
	sprout->version = LittleLong(sprin->version);
	sprout->numframes = LittleLong(sprin->numframes);

	if (sprout->version != SPRITE_VERSION)
		ri.Sys_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", mod->name, sprout->version, SPRITE_VERSION);

	if (sprout->numframes > SPRITE_MAX_FRAMES)
		ri.Sys_Error(ERR_DROP, "%s has too many frames (%i > %i)", mod->name, sprout->numframes, SPRITE_MAX_FRAMES);

	/* byte swap everything */
	for (i = 0; i < sprout->numframes; i++) {
		sprout->frames[i].width = LittleLong(sprin->frames[i].width);
		sprout->frames[i].height = LittleLong(sprin->frames[i].height);
		sprout->frames[i].origin_x = LittleLong(sprin->frames[i].origin_x);
		sprout->frames[i].origin_y = LittleLong(sprin->frames[i].origin_y);
		memcpy(sprout->frames[i].name, sprin->frames[i].name, SPRITE_MAX_SKINNAME);
		mod->alias.skins_img[i] = GL_FindImage(sprout->frames[i].name, it_sprite);
	}

	mod->alias.extraData = sprout;
	mod->type = mod_sprite;
}
