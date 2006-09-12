/**
 * @file gl_anim.c
 * @brief animation parsing and playing
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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
#define LNEXT(x)	((x+1 < MAX_ANIMLIST) ? x+1 : 0)

/**
 * @brief
 */
manim_t *Anim_Get(model_t * mod, char *name)
{
	manim_t *anim;
	int i;

	if (!mod || mod->type != mod_alias)
		return NULL;

	for (i = 0, anim = mod->animdata; i < mod->numanims; i++, anim++)
		if (!Q_strncmp(name, anim->name, MAX_ANIMNAME))
			return anim;

	ri.Con_Printf(PRINT_ALL, "model \"%s\" doesn't have animation \"%s\"\n", mod->name, name);
	return NULL;
}


/**
 * @brief
 */
void Anim_Append(animState_t * as, model_t * mod, char *name)
{
	manim_t *anim;

	assert(as->ladd < MAX_ANIMLIST);
#ifdef DEBUG
	if (as->ladd >= MAX_ANIMLIST)
		return;					/* never reached. need for code analyst. */
#endif

	if (!mod || mod->type != mod_alias)
		return;

	/* get animation */
	anim = Anim_Get(mod, name);
	if (!anim)
		return;

	if (as->lcur == as->ladd) {
		/* first animation */
		as->oldframe = anim->from;
		if (anim->to > anim->from)
			as->frame = anim->from + 1;
		else
			as->frame = anim->from;

		as->backlerp = 0.0;
		as->time = anim->time;
		as->dt = 0;

		as->list[as->ladd] = anim - mod->animdata;
	} else {
		/* next animation */
		as->list[as->ladd] = anim - mod->animdata;
	}

	/* advance in list (no overflow protection!) */
	as->ladd = LNEXT(as->ladd);
}


/**
 * @brief
 */
void Anim_Change(animState_t * as, model_t * mod, char *name)
{
	manim_t *anim;

	assert(as->ladd < MAX_ANIMLIST);
#ifdef DEBUG
	if (as->ladd >= MAX_ANIMLIST)
		return;					/* never reached. need for code analyst. */
#endif

	if (!mod || mod->type != mod_alias)
		return;

	/* get animation */
	anim = Anim_Get(mod, name);
	if (!anim)
		return;

	if (as->lcur == as->ladd) {
		/* first animation */
		as->oldframe = anim->from;
		if (anim->to > anim->from)
			as->frame = anim->from + 1;
		else
			as->frame = anim->from;

		as->backlerp = 1.0;
		as->time = anim->time;
		as->dt = 0;

		as->list[as->ladd] = anim - mod->animdata;
	} else {
		/* don't change to same animation */
/*		if ( anim == mod->animdata + as->list[as->lcur] ) */
/*			return; */

		/* next animation */
		as->ladd = LNEXT(as->lcur);
		as->list[as->ladd] = anim - mod->animdata;

		if (anim->time < as->time)
			as->time = anim->time;
	}

	/* advance in list (no overflow protection!) */
	as->ladd = LNEXT(as->ladd);
	as->change = qtrue;
}


/**
 * @brief
 */
void Anim_Run(animState_t * as, model_t * mod, int msec)
{
	manim_t *anim;

	assert(as->lcur < MAX_ANIMLIST);
#ifdef DEBUG
	if (as->lcur >= MAX_ANIMLIST)
		return;					/* never reached. need for code analyst. */
#endif

	if (!mod || mod->type != mod_alias)
		return;

	if (as->lcur == as->ladd)
		return;

	as->dt += msec;

	while (as->dt > as->time) {
		as->dt -= as->time;
		anim = mod->animdata + as->list[as->lcur];

		if (as->change || as->frame >= anim->to) {
			/* go to next animation if it isn't the last one */
			if (LNEXT(as->lcur) != as->ladd)
				as->lcur = LNEXT(as->lcur);

			anim = mod->animdata + as->list[as->lcur];

			/* prepare next frame */
			as->dt = 0;
			as->time = anim->time;
			as->oldframe = as->frame;
			as->frame = anim->from;
			as->change = qfalse;
		} else {
			/* next frame of the same animation */
			as->time = anim->time;
			as->oldframe = as->frame;
			as->frame++;
		}
	}

	as->backlerp = 1.0 - (float) as->dt / as->time;
}


/**
 * @brief
 */
char *Anim_GetName(animState_t * as, model_t * mod)
{
	manim_t *anim;

	assert(as->lcur < MAX_ANIMLIST);
#ifdef DEBUG
	if (as->lcur >= MAX_ANIMLIST)
		return NULL;			/* never reached. need for code analyst. */
#endif

	if (!mod || mod->type != mod_alias)
		return NULL;

	if (as->lcur == as->ladd)
		return NULL;

	anim = mod->animdata + as->list[as->lcur];
	return anim->name;
}
