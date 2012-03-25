/**
 * @file r_mesh_anim.c
 * @brief animation parsing and playing
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
#include "r_mesh.h"
#include "r_mesh_anim.h"

#define LOOPNEXT(x) ((x + 1 < MAX_ANIMLIST) ? x + 1 : 0)

/**
 * @brief Adds the given animation to the animation state
 */
static inline void R_AnimAdd (animState_t *as, const model_t *mod, const mAliasAnim_t *anim)
{
	assert(as->ladd < sizeof(as->list));

	as->list[as->ladd] = anim - mod->alias.animdata;

	/* advance in list (no overflow protection!) */
	as->ladd = LOOPNEXT(as->ladd);
}
/**
 * @brief Get the @c mAliasAnim_t for the given animation state
 */
#define R_AnimGetAliasAnim(mod, as) ((mod)->alias.animdata + (as)->list[(as)->lcur])

/**
 * @brief Searches a given animation id in the given model data
 * @sa R_AnimChange
 * @sa R_AnimAppend
 * @sa R_AnimRun
 * @sa R_AnimGetName
 * @param[in] mod The model to search the given animation id in
 * @param[in] name The animation name (from the *.anm file) to search
 * @return @c NULL if not found, otherwise the animation data
 */
static const mAliasAnim_t *R_AnimGet (const model_t * mod, const char *name)
{
	mAliasAnim_t *anim;
	int i;

	if (!mod || !mod->alias.num_anims)
		return NULL;

	for (i = 0, anim = mod->alias.animdata; i < mod->alias.num_anims; i++, anim++)
		if (Q_streq(name, anim->name))
			return anim;

	Com_Printf("model \"%s\" doesn't have animation \"%s\"\n", mod->name, name);
	return NULL;
}

/**
 * @brief Appends a new animation to the current running one
 * @sa R_AnimGet
 * @sa R_AnimChange
 * @sa R_AnimRun
 * @sa R_AnimGetName
 * @param[in,out] as The animation state to append the new animation to
 * @param[in] mod The model to append the animation for
 * @param[in] name The animation name (from the *.anm file) to append
 */
void R_AnimAppend (animState_t * as, const model_t * mod, const char *name)
{
	const mAliasAnim_t *anim;

	if (!mod || !mod->alias.num_anims)
		return;

	/* get animation */
	anim = R_AnimGet(mod, name);
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

		as->change = qtrue;
	}

	R_AnimAdd(as, mod, anim);
}


/**
 * @brief Changes the animation for md2 models
 * @sa R_AnimGet
 * @sa R_AnimAppend
 * @sa R_AnimRun
 * @sa R_AnimGetName
 * @param[in,out] as Client side animation state of the model
 * @param[in] mod Model structure to change the animation for
 * @param[in] name Animation state name to switch to
 */
void R_AnimChange (animState_t * as, const model_t * mod, const char *name)
{
	const mAliasAnim_t *anim;

	if (!mod) {
		Com_Printf("R_AnimChange: No model given (%s)\n", name);
		return;
	}

	if (!mod->alias.num_anims) {
		Com_Printf("R_AnimChange: Model with no animations (%s) (model: %s)\n", name, mod->name);
		return;
	}

	/* get animation */
	anim = R_AnimGet(mod, name);
	if (!anim) {
		Com_Printf("R_AnimChange: No such animation: %s (model: %s)\n", name, mod->name);
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

		as->change = qtrue;
	} else {
		/* next animation */
		as->ladd = LOOPNEXT(as->lcur);
		if (anim->time < as->time)
			as->time = anim->time;
		/* don't change to the same animation */
		if (anim != R_AnimGetAliasAnim(mod, as))
			as->change = qtrue;
	}

	R_AnimAdd(as, mod, anim);
}


/**
 * @brief Run the animation of the given model
 * @sa R_AnimGet
 * @sa R_AnimAppend
 * @sa R_AnimChange
 * @sa R_AnimGetName
 * @param[in,out] as The animation state to run
 * @param[in] mod The model to run the animation for
 * @param[in] msec The milliseconds since this was called last
 */
void R_AnimRun (animState_t * as, const model_t * mod, int msec)
{
	assert(as->lcur < MAX_ANIMLIST);

	if (!mod || !mod->alias.num_anims)
		return;

	if (as->lcur == as->ladd)
		return;

	as->dt += msec;

	while (as->dt > as->time) {
		const mAliasAnim_t *anim = R_AnimGetAliasAnim(mod, as);
		as->dt -= as->time;

		if (as->change || as->frame >= anim->to) {
			/* go to next animation if it isn't the last one */
			if (LOOPNEXT(as->lcur) != as->ladd)
				as->lcur = LOOPNEXT(as->lcur);

			anim = R_AnimGetAliasAnim(mod, as);

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
 * @brief Get the current running animation for a model
 * @sa R_AnimGet
 * @sa R_AnimAppend
 * @sa R_AnimRun
 * @sa R_AnimChange
 * @param[in] as The animation state to check
 * @param[in] mod The model to check
 * @return @c NULL if no animation is set or running or the name of the current running animation otherwise.
 */
const char *R_AnimGetName (const animState_t * as, const model_t * mod)
{
	const mAliasAnim_t *anim;

	assert(as->lcur < MAX_ANIMLIST);

	if (!mod || !mod->alias.num_anims)
		return NULL;

	if (as->lcur == as->ladd)
		return NULL;

	anim = R_AnimGetAliasAnim(mod, as);
	return anim->name;
}
