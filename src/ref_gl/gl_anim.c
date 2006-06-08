/* gl_anim.c -- animation parsing and playing */

#include "gl_local.h"

#define LNEXT(x)	((x+1 < MAX_ANIMLIST) ? x+1 : 0)

/*
===============
Anim_Get
===============
*/
manim_t *Anim_Get( model_t *mod, char *name )
{
	manim_t	*anim;
	int		i;

	if ( !mod || mod->type != mod_alias ) return NULL;

	for ( i = 0, anim = mod->animdata; i < mod->numanims; i++, anim++ )
		if ( !strcmp( name, anim->name ) )
			return anim;

	ri.Con_Printf( PRINT_ALL, "model \"%s\" doesn't have animation \"%s\"\n", mod->name, name );
	return NULL;
}


/*
===============
Anim_Append
===============
*/
void Anim_Append( animState_t *as, model_t *mod, char *name )
{
	manim_t	*anim;

	if ( !mod || mod->type != mod_alias ) return;

	/* get animation */
	anim = Anim_Get( mod, name );
	if ( !anim ) return;

	if ( as->lcur == as->ladd )
	{
		/* first animation */
		as->oldframe = anim->from;
		if ( anim->to > anim->from ) as->frame = anim->from+1;
		else as->frame = anim->from;

		as->backlerp = 0.0;
		as->time = anim->time;
		as->dt = 0;

		as->list[as->ladd] = anim - mod->animdata;
	}
	else
	{
		/* next animation */
		as->list[as->ladd] = anim - mod->animdata;
	}

	/* advance in list (no overflow protection!) */
	as->ladd = LNEXT( as->ladd );
}


/*
===============
Anim_Change
===============
*/
void Anim_Change( animState_t *as, model_t *mod, char *name )
{
	manim_t	*anim;

	if ( !mod || mod->type != mod_alias ) return;

	/* get animation */
	anim = Anim_Get( mod, name );
	if ( !anim ) return;

	if ( as->lcur == as->ladd )
	{
		/* first animation */
		as->oldframe = anim->from;
		if ( anim->to > anim->from ) as->frame = anim->from+1;
		else as->frame = anim->from;

		as->backlerp = 1.0;
		as->time = anim->time;
		as->dt = 0;

		as->list[as->ladd] = anim - mod->animdata;
	}
	else
	{
		/* don't change to same animation */
/*		if ( anim == mod->animdata + as->list[as->lcur] ) */
/*			return; */

		/* next animation */
		as->ladd = LNEXT( as->lcur );
		as->list[as->ladd] = anim - mod->animdata;

		if ( anim->time < as->time )
			as->time = anim->time;
	}

	/* advance in list (no overflow protection!) */
	as->ladd = LNEXT( as->ladd );
	as->change = qtrue;
}


/*
===============
Anim_Run
===============
*/
void Anim_Run( animState_t *as, model_t *mod, int msec )
{
	manim_t	*anim;

	if ( !mod || mod->type != mod_alias ) return;

	if ( as->lcur == as->ladd )
		return;

	as->dt += msec;

	while ( as->dt > as->time ) 
	{
		as->dt -= as->time;
		anim = mod->animdata + as->list[as->lcur];

		if ( as->change || as->frame >= anim->to )
		{
			/* go to next animation if it isn't the last one */
			if ( LNEXT( as->lcur ) != as->ladd ) 
				as->lcur = LNEXT( as->lcur );

			anim = mod->animdata + as->list[as->lcur];

			/* prepare next frame */
			as->dt = 0;
			as->time = anim->time;
			as->oldframe = as->frame;
			as->frame = anim->from;
			as->change = qfalse;
		}
		else
		{
			/* next frame of the same animation */
			as->time = anim->time;
			as->oldframe = as->frame;
			as->frame++;
		}
	}

	as->backlerp = 1.0 - (float)as->dt / as->time;
}


/*
===============
Anim_GetName
===============
*/
char *Anim_GetName( animState_t *as, model_t *mod )
{
	manim_t	*anim;

	if ( !mod || mod->type != mod_alias ) return NULL;

	if ( as->lcur == as->ladd )
		return NULL;

	anim = mod->animdata + as->list[as->lcur];
	return anim->name;
}
