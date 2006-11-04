// cl_le.c -- local entity management

#include "client.h"

//
lm_t	LMs[MAX_LOCALMODELS];
int		numLMs;
//
le_t	LEs[MAX_EDICTS];
int		numLEs;
//

vec3_t player_mins = {-PLAYER_WIDTH, -PLAYER_WIDTH, -PLAYER_MIN };
vec3_t player_maxs = { PLAYER_WIDTH,  PLAYER_WIDTH,  PLAYER_STAND };


//===========================================================================
//
// LM drawing (that's all local models do)
//
//===========================================================================



/*
==============
LM_AddToScene
==============
*/
void LM_AddToScene( void )
{
	lm_t		*lm;
	entity_t	ent;
	int		i;

	memset( &ent, 0, sizeof(entity_t) );	

	for ( i = 0, lm = LMs; i < numLMs; i++, lm++ ) 
	{
		// check for visibility
		if ( !((1 << (int)cl_worldlevel->value) & lm->levelflags) )
			continue;

		// set entity values
		VectorCopy( lm->origin, ent.origin );
		VectorCopy( lm->origin, ent.oldorigin );
		VectorCopy( lm->angles, ent.angles );
		ent.model = lm->model;
		ent.sunfrac = lm->sunfrac;

		// add it to the scene
		V_AddEntity( &ent );
	}
}


//===========================================================================
//
// LE thinking
//
//===========================================================================


/*
==============
LE_Think
==============
*/
void LE_Think( void )
{
	le_t	*le;
	int		i;

	for ( i = 0, le = LEs; i < numLEs; i++, le++ )
		if ( le->inuse && le->think )
			// call think function
			le->think( le );
}


//===========================================================================
//
// LE think functions
//
//===========================================================================

char retAnim[64];

/*
==============
LE_GetAnim
==============
*/
char *LE_GetAnim( char *anim, inventory_t *i, int state )
{
	char		*mod;
	qboolean	akimbo;
	char		category, *type;

	mod = retAnim;

	// add crouched flag
	if ( state & STATE_CROUCHED ) *mod++ = 'c';

	// determine relevant data
	akimbo = false;
	if ( i->right.t == NONE ) 
	{
		category = '0';
		if ( i->left.t == NONE ) type = "item";
		else { akimbo = true; type = csi.ods[i->left.t].type; }
	} else {
		category = csi.ods[i->right.t].category;
		type = csi.ods[i->right.t].type;
		if ( i->left.t != NONE ) akimbo = true;
	}

	if ( !strcmp( anim, "stand" ) || !strcmp( anim, "walk" ) )
	{
		strcpy( mod, anim );
		mod += strlen( anim );
		*mod++ = category;
		*mod++ = 0;
	} else {
		strcpy( mod, anim );
		mod += strlen( anim );
		*mod++ = '_';
		strcpy( mod, type );
		mod += strlen( type );
		if ( akimbo ) strcpy( mod, "_d" );
	}

	return retAnim;
}


/*
==============
LET_StartIdle
==============
*/
void LET_StartIdle( le_t *le )
{
	if ( le->state & STATE_DEAD )
		re.AnimChange( &le->as, le->model1, va( "dead%i", le->state & STATE_DEAD ) );
	else if ( le->state & STATE_PANIC )
		re.AnimChange( &le->as, le->model1, "panic0" );
	else
		re.AnimChange( &le->as, le->model1, LE_GetAnim( "stand", &le->i, le->state ) );

	le->think = NULL;
}


/*
==============
LET_DoAppear
==============
*/
/*#define APPEAR_TIME		1000

void LET_DoAppear( le_t *le )
{
	if ( cl.time > le->startTime + APPEAR_TIME )
	{
		le->alpha = 1.0;
		le->think = NULL;
		return;
	}

	le->alpha = (float)(cl.time - le->startTime) / APPEAR_TIME;
}*/


/*
==============
LET_Appear
==============
*/
/*void LET_Appear( le_t *le )
{
	LET_StartIdle( le );

	le->startTime = cl.time;
	le->think = LET_DoAppear;
	le->think( le );
}*/


/*
==============
LET_Perish
==============
*/
/*#define PERISH_TIME		1000

void LET_Perish( le_t *le )
{
	if ( cl.time > le->startTime + PERISH_TIME )
	{
		le->inuse = false;
		return;
	}

	le->alpha = 1.0 - (float)(cl.time - le->startTime) / PERISH_TIME;
}*/


/*
==============
LET_PathMove
==============
*/
void LET_PathMove( le_t *le )
{
	byte	dv;
	float	frac;
	vec3_t	start, dest, delta;

	// check for start
	if ( cl.time <= le->startTime )
		return;

	// move ahead
	while ( cl.time > le->endTime )
	{
		VectorCopy( le->pos, le->oldPos );

		if ( le->pathPos < le->pathLength )
		{
			// next part
			dv = le->path[le->pathPos++];
			PosAddDV( le->pos, dv );
			le->dir = dv&7;
			le->angles[YAW] = dangle[le->dir];
			le->startTime = le->endTime;
			le->endTime += ((dv&7) > 3 ? US*1.41 : US) * 1000 / le->speed;
		}
		else
		{
			// end of move
			le_t *floor;
			Grid_PosToVec( le->pos, le->origin );

			// calculate next possible moves
			CL_BuildForbiddenList();
			if ( selActor == le ) 
				Grid_MoveCalc( le->pos, MAX_ROUTE, fb_list, fb_length );

			floor = LE_Find( ET_ITEM, le->pos );
			if ( floor ) le->i.floor = &floor->i;

			blockEvents = false;
			le->think = LET_StartIdle;
			le->think( le );
			return;
		}
	}

	// interpolate the position
	Grid_PosToVec( le->oldPos, start );
	Grid_PosToVec( le->pos, dest );
	VectorSubtract( dest, start, delta );

	frac = (float)(cl.time - le->startTime) / (float)(le->endTime - le->startTime);

	VectorMA( start, frac, delta, le->origin );
}

/*
==============
LET_StartPathMove
==============
*/
void LET_StartPathMove( le_t *le )
{
	re.AnimChange( &le->as, le->model1, LE_GetAnim( "walk", &le->i, le->state ) );

	le->think = LET_PathMove;
	le->think( le );
}


/*
==============
LET_Shoot
==============
*/
void LET_Shoot( le_t *le )
{
	if ( cl.time < le->endTime )
		return;

	le->think = NULL;
	re.AnimChange( &le->as, le->model1, LE_GetAnim( "stand", &le->i, le->state ) );
}

/*
==============
LET_StartShoot
==============
*/
void LET_StartShoot( le_t *le )
{
	re.AnimAppend( &le->as, le->model1, LE_GetAnim( "shoot", &le->i, le->state ) );

	le->endTime = cl.time + 400;
	le->think = LET_Shoot;
}

/*
==============
LET_Projectile
==============
*/
void LET_Projectile( le_t *le )
{
	if ( cl.time >= le->endTime )
	{
		// impact
		le->ptl->inuse = false;
		if ( le->ref1 )
		{
			if ( le->ref2[0] ) S_StartLocalSound( le->ref2 );

			le->ptl = CL_ParticleSpawn( le->ref1, le->maxs, bytedirs[le->state], NULL );
			VecToAngles( bytedirs[le->state], le->ptl->angles );
		}
		le->inuse = false;
		return;
	}

	// kinematics
	VectorMA( le->origin, cls.frametime, le->mins, le->origin );
	VectorCopy( le->origin, le->ptl->s );
}

//===========================================================================
//
// LE Special Effects
//
//===========================================================================

void LE_AddProjectile( fireDef_t *fd, byte flags, vec3_t muzzle, vec3_t impact, byte normal )
{
	le_t	*le;
	vec3_t	delta;
	float	dist;

	// add le
	le = LE_Add( 0 );
	le->invis = true;

	// bind particle
	le->ptl = CL_ParticleSpawn( fd->projectile, muzzle, NULL, NULL );
	if ( !le->ptl )
	{
		le->inuse = false;
		return;
	}

	// calculate parameters
	VectorSubtract( impact, muzzle, delta );
	dist = VectorLength( delta );

	VecToAngles( delta, le->ptl->angles );
	le->state = normal;

	if ( !fd->speed )
	{
		// infinite speed projectile
		ptl_t	*ptl;
		le->inuse = false;
		le->ptl->size[0] = dist;
		VectorMA( muzzle, 0.5, delta, le->ptl->s );
		if ( flags & SF_IMPACT )
		{
			if ( fd->impactSound[0] ) S_StartLocalSound( fd->impactSound );
			ptl = CL_ParticleSpawn( fd->impact, impact, bytedirs[normal], NULL );
			VecToAngles( bytedirs[normal], ptl->angles );
		}
		return;
	}
	
	VectorCopy( impact, le->maxs );
	VectorScale( delta, (fd->speed / dist), le->mins );

	VectorMA( muzzle, le->ptl->size[0] / (2*dist), delta, le->origin );
	le->endTime = cl.time + (1000.0 * (dist - le->ptl->size[0]) / fd->speed);

	// think function
	if ( flags & SF_IMPACT ) 
	{
		le->ref1 = fd->impact;
		le->ref2 = fd->impactSound;
	}
	le->think = LET_Projectile;
	le->think( le );
}

//===========================================================================
//
// LE Management functions
//
//===========================================================================


/*
==============

LE_Add
==============
*/
le_t *LE_Add( int entnum )
{
	int		i;
	le_t	*le;

	for ( i = 0, le = LEs; i < numLEs; i++, le++ )
		if ( !le->inuse )
			// found a free LE
			break;

	// list full, try to make list longer
	if ( i == numLEs ) 
	{
		if ( numLEs >= MAX_EDICTS - numLMs )
		{
			// no free LEs
			Com_Error( ERR_DROP, "Too many LEs\n" );
			return NULL;
		}

		// list isn't too long
		numLEs++;
	}

	// initialize the new LE
	memset( le, 0, sizeof( le_t ) );
	le->inuse = true;
	le->entnum = entnum;
	return le;
}

/*
==============
LE_Get
==============
*/
le_t *LE_Get( int entnum )
{
	int		i;
	le_t	*le;

	for ( i = 0, le = LEs; i < numLEs; i++, le++ )
		if ( le->inuse && le->entnum == entnum )
			// found the LE
			return le;

	// didn't find it
	return NULL;
}


/*
==============
LE_Find
==============
*/
le_t *LE_Find( int type, pos3_t pos )
{
	int		i;
	le_t	*le;

	for ( i = 0, le = LEs; i < numLEs; i++, le++ )
		if ( le->inuse && le->type == type && VectorCompare( le->pos, pos ) )
			// found the LE
			return le;

	// didn't find it
	return NULL;
}


/*
==============
LE_AddToScene
==============
*/
void LE_AddToScene( void )
{
	le_t		*le;
	entity_t	ent;
	vec3_t		sunOrigin;
	int		i;

	for ( i = 0, le = LEs; i < numLEs; i++, le++ ) 
	{
		if ( le->inuse && !le->invis && le->pos[2] <= cl_worldlevel->value ) 
		{
			memset( &ent, 0, sizeof(entity_t) );	

			// calculate sun lighting
			if ( !VectorCompare( le->origin, le->oldOrigin ) )
			{
				VectorCopy( le->origin, le->oldOrigin );
				VectorMA( le->origin, 512, map_sun.dir, sunOrigin );
				if ( !CM_TestLine( le->origin, sunOrigin ) )
					le->sunfrac = 1.0f;
				else
					le->sunfrac = 0.0f;
			}
			ent.sunfrac = le->sunfrac;
			ent.alpha = le->alpha;

			// set entity values
			VectorCopy( le->origin, ent.origin );
			VectorCopy( le->origin, ent.oldorigin );
			VectorCopy( le->angles, ent.angles );
			ent.model = le->model1;
			ent.skinnum = le->skinnum;

			// do animation
			re.AnimRun( &le->as, ent.model, cls.frametime*1000 );
			ent.as = le->as;
			
			// call add function
			// if it returns false, don't draw
			if ( le->addFunc )
				if ( !le->addFunc( le, &ent ) )
					continue;

			// add it to the scene
			V_AddEntity( &ent );
		}
	}
}


//===========================================================================
//
// LE Tracing
//
//===========================================================================


typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		trace;
	le_t		*passle;
	int			contentmask;
} moveclip_t;


/*
====================
CL_ClipMoveToLEs

====================
*/
void CL_ClipMoveToLEs ( moveclip_t *clip )
{
	int			i;
	le_t		*le;
	trace_t		trace;
	int			headnode;

	if ( clip->trace.allsolid )
		return;

	for ( i = 0, le = LEs; i < numLEs; i++, le++ ) 
	{
		if ( !le->inuse || !(le->contents & clip->contentmask) )
			continue;
		if ( le == clip->passle )
			continue;

		// might intersect, so do an exact clip
		headnode = CM_HeadnodeForBox (le->mins, le->maxs);

		trace = CM_TransformedBoxTrace (clip->start, clip->end,
			clip->mins, clip->maxs, headnode,  clip->contentmask,
			le->origin, vec3_origin );

		if (trace.allsolid || trace.startsolid ||
		trace.fraction < clip->trace.fraction)
		{
			trace.le = le;
			clip->trace = trace;
		 	if (clip->trace.startsolid) clip->trace.startsolid = true;
		}
		else if (trace.startsolid)
			clip->trace.startsolid = true;
	}
}


/*
==================
CL_TraceBounds
==================
*/
void CL_TraceBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
	int		i;
	
	for (i=0 ; i<3 ; i++)
	{
		if (end[i] > start[i])
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

/*
==================
CL_Trace

Moves the given mins/maxs volume through the world from start to end.

Passedict and edicts owned by passedict are explicitly not checked.

==================
*/
trace_t CL_Trace (vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, le_t *passle, int contentmask)
{
	moveclip_t		clip;
	
	// clip to world
	clip.trace = CM_CompleteBoxTrace (start, end, mins, maxs, (1<<((int)cl_worldlevel->value+1))-1, contentmask);
	clip.trace.le = NULL;
	if ( clip.trace.fraction == 0 )
		return clip.trace;		// blocked by the world

	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passle = passle;

	// create the bounding box of the entire move
	CL_TraceBounds ( start, mins, maxs, end, clip.boxmins, clip.boxmaxs );

	// clip to other solid entities
	CL_ClipMoveToLEs ( &clip );

	return clip.trace;
}
