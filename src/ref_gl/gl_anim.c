/**
 * @file gl_anim.c
 * @brief animation parsing and playing
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
 * @sa Anim_Change
 * @sa Anim_Append
 * @sa Anim_Run
 * @sa Anim_GetName
 */
static mAliasAnim_t *Anim_Get (model_t * mod, const char *name)
{
	mAliasAnim_t *anim;
	int i;

	if (!mod || mod->type != mod_alias_md2)
		return NULL;

	for (i = 0, anim = mod->alias.animdata; i < mod->alias.numanims; i++, anim++)
		if (!Q_strncmp(name, anim->name, MAX_ANIMNAME))
			return anim;

	ri.Con_Printf(PRINT_ALL, "model \"%s\" doesn't have animation \"%s\"\n", mod->name, name);
	return NULL;
}


/**
 * @brief
 * @sa Anim_Get
 * @sa Anim_Change
 * @sa Anim_Run
 * @sa Anim_GetName
 */
void Anim_Append (animState_t * as, model_t * mod, const char *name)
{
	mAliasAnim_t *anim;

	assert(as->ladd < MAX_ANIMLIST);
#ifdef DEBUG
	if (as->ladd >= MAX_ANIMLIST)
		return;					/* never reached. need for code analyst. */
#endif

	if (!mod || mod->type != mod_alias_md2)
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

		as->list[as->ladd] = anim - mod->alias.animdata;
	} else {
		/* next animation */
		as->list[as->ladd] = anim - mod->alias.animdata;
	}

	/* advance in list (no overflow protection!) */
	as->ladd = LNEXT(as->ladd);
}


/**
 * @brief Changes the animation for md2 models
 * @sa Anim_Get
 * @sa Anim_Append
 * @sa Anim_Run
 * @sa Anim_GetName
 * @param[in] as Client side animation state of the model
 * @param[in] mod Model structure to change the animation for (md2/mod_alias_md2)
 * @param[in] name Animation state name to switch to
 */
void Anim_Change (animState_t * as, model_t * mod, const char *name)
{
	mAliasAnim_t *anim;

	assert(as->ladd < MAX_ANIMLIST);
#ifdef DEBUG
	if (as->ladd >= MAX_ANIMLIST)
		return;					/* never reached. need for code analyst. */
#endif

	if (!mod || mod->type != mod_alias_md2) {
		ri.Con_Printf(PRINT_ALL, "Anim_Change: No md2 model - can't set animation (model: %s)\n", mod->name);
		return;
	}

	/* get animation */
	anim = Anim_Get(mod, name);
	if (!anim) {
		ri.Con_Printf(PRINT_ALL, "Anim_Change: No such animation: %s (model: %s)\n", name, mod->name);
		return;
	}

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

		as->list[as->ladd] = anim - mod->alias.animdata;
	} else {
		/* don't change to same animation */
/*		if ( anim == mod->animdata + as->list[as->lcur] ) */
/*			return; */

		/* next animation */
		as->ladd = LNEXT(as->lcur);
		as->list[as->ladd] = anim - mod->alias.animdata;

		if (anim->time < as->time)
			as->time = anim->time;
	}

	/* advance in list (no overflow protection!) */
	as->ladd = LNEXT(as->ladd);
	as->change = qtrue;
}


/**
 * @brief Run the animation of the given md2 model
 * @sa Anim_Get
 * @sa Anim_Append
 * @sa Anim_Change
 * @sa Anim_GetName
 */
void Anim_Run (animState_t * as, model_t * mod, int msec)
{
	mAliasAnim_t *anim;

	assert(as->lcur < MAX_ANIMLIST);
#ifdef DEBUG
	if (as->lcur >= MAX_ANIMLIST)
		return;					/* never reached. need for code analyst. */
#endif

	if (!mod || mod->type != mod_alias_md2)
		return;

	if (as->lcur == as->ladd)
		return;

	as->dt += msec;

	while (as->dt > as->time) {
		as->dt -= as->time;
		anim = mod->alias.animdata + as->list[as->lcur];

		if (as->change || as->frame >= anim->to) {
			/* go to next animation if it isn't the last one */
			if (LNEXT(as->lcur) != as->ladd)
				as->lcur = LNEXT(as->lcur);

			anim = mod->alias.animdata + as->list[as->lcur];

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
 * @sa Anim_Get
 * @sa Anim_Append
 * @sa Anim_Run
 * @sa Anim_Change
 */
char *Anim_GetName (animState_t * as, model_t * mod)
{
	mAliasAnim_t *anim;

	assert(as->lcur < MAX_ANIMLIST);
#ifdef DEBUG
	if (as->lcur >= MAX_ANIMLIST)
		return NULL;			/* never reached. need for code analyst. */
#endif

	if (!mod || mod->type != mod_alias_md2)
		return NULL;

	if (as->lcur == as->ladd)
		return NULL;

	anim = mod->alias.animdata + as->list[as->lcur];
	return anim->name;
}
