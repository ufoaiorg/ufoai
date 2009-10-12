/**
 * @file sv_world.c
 * @brief World query functions.
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/server/sv_world.c
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

#include "server.h"
#include "../common/tracing.h"

/** @brief static mesh models (none-animated) can have a server side flag set to be clipped for pathfinding */
typedef struct sv_model_s {
	vec3_t mins, maxs;	/**< the mins and maxs of the model bounding box */
	int frame;			/**< the frame the mins and maxs were calculated for */
	char *name;			/**< the model path (relative to base/ */
} sv_model_t;

static sv_model_t sv_models[MAX_MOD_KNOWN];
static int sv_numModels;

/*
===============================================================================
ENTITY AREA CHECKING
===============================================================================
*/
/** @todo this use of "area" is different from the bsp file use */

/** @todo remove this mess! */
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (ptrdiff_t)&(((t *)0)->m)))

#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edict_t,area)

typedef struct areanode_s {
	int axis;					/**< -1 = leaf node */
	float dist;
	struct areanode_s *children[2];
	link_t triggerEdicts;
	link_t solidEdicts;
} areanode_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	32

static areanode_t sv_areaNodes[AREA_NODES];
static int sv_numAreaNodes;

static float *areaMins, *areaMaxs;
static edict_t **areaEdictList;
static int areaEdictListCount, areaEdictListMaxCount;

/**
 * @brief ClearLink is used for new headnodes
 */
static void ClearLink (link_t * l)
{
	l->prev = l->next = l;
}

static void RemoveLink (link_t * l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

static void InsertLinkBefore (link_t * l, link_t * before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

/**
 * @brief Builds a uniformly subdivided tree for the given world size
 * @sa SV_ClearWorld
 * @sa SV_LinkEdict
 * @param[in] mins
 * @param[in] maxs
 * @param[in] depth
 */
static areanode_t *SV_CreateAreaNode (int depth, const vec3_t mins, const vec3_t maxs)
{
	areanode_t *anode;
	vec3_t size;
	vec3_t mins1, maxs1, mins2, maxs2;

	anode = &sv_areaNodes[sv_numAreaNodes];
	sv_numAreaNodes++;

	ClearLink(&anode->triggerEdicts);
	ClearLink(&anode->solidEdicts);

	if (depth == AREA_DEPTH) {
		anode->axis = LEAFNODE; /* end of tree */
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract(maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = PLANE_X;
	else
		anode->axis = PLANE_Y;

	anode->dist = 0.5f * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy(mins, mins1);
	VectorCopy(mins, mins2);
	VectorCopy(maxs, maxs1);
	VectorCopy(maxs, maxs2);

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateAreaNode(depth + 1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode(depth + 1, mins1, maxs1);

	return anode;
}

/**
 * @brief called after the world model has been loaded, before linking any entities
 * @sa SV_SpawnServer
 * @sa SV_CreateAreaNode
 */
void SV_ClearWorld (void)
{
	int i;

	for (i = 0; i < sv_numModels; i++)
		if (sv_models[i].name)
			Mem_Free(sv_models[i].name);

	memset(sv_models, 0, sizeof(sv_models));
	sv_numModels = 0;

	memset(sv_areaNodes, 0, sizeof(sv_areaNodes));
	sv_numAreaNodes = 0;
	SV_CreateAreaNode(0, mapMin, mapMax);
}


/**
 * @brief call before removing an entity, and before trying to move one, so it doesn't clip against itself
 */
void SV_UnlinkEdict (edict_t * ent)
{
	if (!ent->area.prev)
		return;					/* not linked in anywhere */
	RemoveLink(&ent->area);
	ent->area.prev = ent->area.next = NULL;
	if (ent->child)
		SV_UnlinkEdict(ent->child);
}

/**
 * @brief Needs to be called any time an entity changes origin, mins, maxs,
 * or solid. Automatically unlinks if needed. Sets ent->absmin and ent->absmax
 * @sa SV_CreateAreaNode
 */
void SV_LinkEdict (edict_t * ent)
{
	areanode_t *node;

	if (ent->area.prev)
		SV_UnlinkEdict(ent);	/* unlink from old position */

	if (ent == ge->edicts)
		return;					/* don't add the world */

	if (!ent->inuse)
		return;

	/* set the size */
	VectorSubtract(ent->maxs, ent->mins, ent->size);

	ent->linkcount++;

	/* expand for rotation */
	if (ent->solid == SOLID_BSP && VectorNotEmpty(ent->angles)) {
		vec3_t minVec, maxVec;
		vec3_t centerVec, halfVec, worstVec, newCenterVec;
		vec3_t m[3];

		/* Find the center of the extents. */
		VectorCenterFromMinsMaxs(ent->mins, ent->maxs, centerVec);

		/* Find the half height and half width of the extents. */
		VectorSubtract(ent->maxs, centerVec, halfVec);

		/* Define the rough largest extenst that this box can have when rotated. */
		worstVec[0] = worstVec[1] = worstVec[2] = halfVec[0] + halfVec[1] + halfVec[2];

		/* Rotate the center about the origin. */
		AngleVectors(ent->angles, m[0], m[1], m[2]);
		VectorRotate(m, centerVec, newCenterVec);

		/* Set minVec and maxVec to bound around newCenterVec at halfVec size. */
		VectorSubtract(newCenterVec, halfVec, minVec);
		VectorAdd(newCenterVec, halfVec, maxVec);

		/* Adjust the absolute mins/maxs */
		VectorAdd(ent->origin, minVec, ent->absmin);
		VectorAdd(ent->origin, maxVec, ent->absmax);
	} else {  /* normal */
		VectorAdd(ent->origin, ent->mins, ent->absmin);
		VectorAdd(ent->origin, ent->maxs, ent->absmax);
	}

	if (ent->solid == SOLID_NOT)
		return;

	/* find the first node that the ent's box crosses */
	node = sv_areaNodes;
	while (1) {
		/* end of tree */
		if (node->axis == LEAFNODE)
			break;
		if (ent->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;				/* crosses the node */
	}

	/* link it in */
	if (ent->solid == SOLID_TRIGGER)
		InsertLinkBefore(&ent->area, &node->triggerEdicts);
	else
		InsertLinkBefore(&ent->area, &node->solidEdicts);

	/* If this ent has a child, link it back in, too */
	if (ent->child) {
		VectorCopy(ent->absmin, ent->child->mins);
		VectorCopy(ent->absmax, ent->child->maxs);

		/* expand the trigger box */
		ent->child->mins[0] -= (UNIT_SIZE / 2);
		ent->child->mins[1] -= (UNIT_SIZE / 2);
		ent->child->maxs[0] += (UNIT_SIZE / 2);
		ent->child->maxs[1] += (UNIT_SIZE / 2);

		/* link child back into the world */
		SV_LinkEdict(ent->child);
	}
}


/**
 * @brief fills in a table of edict pointers with edicts that have bounding boxes
 * that intersect the given area. It is possible for a non-axial bmodel
 * to be returned that doesn't actually intersect the area on an exact test.
 * @sa SV_AreaEdicts
 * @param node
 * @param areaType @c AREA_SOLID or @c AREA_TRIGGERS
 */
static void SV_AreaEdicts_r (areanode_t * node, int areaType)
{
	link_t *l, *next, *start;
	edict_t *check;
	int count;

	count = 0;

	/* touch linked edicts */
	if (areaType == AREA_SOLID)
		start = &node->solidEdicts;
	else
		start = &node->triggerEdicts;

	for (l = start->next; l != start; l = next) {
		next = l->next;
		check = EDICT_FROM_AREA(l);
		/*Com_Printf("check->number: %i - headnode: %i\n", check->number, check->headnode);*/

		if (check->solid == SOLID_NOT)
			continue;			/* deactivated */
		if (check->absmin[0] > areaMaxs[0]
			|| check->absmin[1] > areaMaxs[1]
			|| check->absmin[2] > areaMaxs[2]
			|| check->absmax[0] < areaMins[0]
			|| check->absmax[1] < areaMins[1]
			|| check->absmax[2] < areaMins[2])
			continue;			/* not touching */

		if (areaEdictListCount == areaEdictListMaxCount) {
			Com_Printf("SV_AreaEdicts_r: MAXCOUNT\n");
			return;
		}

		areaEdictList[areaEdictListCount] = check;
		areaEdictListCount++;
	}

	if (node->axis == LEAFNODE)
		return;					/* terminal node - end of tree */

	/* recurse down both sides */
	if (areaMaxs[node->axis] > node->dist)
		SV_AreaEdicts_r(node->children[0], areaType);
	if (areaMins[node->axis] < node->dist)
		SV_AreaEdicts_r(node->children[1], areaType);
}

/**
 * @sa SV_AreaEdicts_r
 * @param[in] mins The mins of the bounding box
 * @param[in] maxs The maxs of the bounding box
 * @param[out] list The edict list that this trace is hitting
 * @param[in] maxCount The size of the given @c list
 * @param[in] areaType @c AREA_TRIGGERS or @ AREA_SOLID
 * @return the number of pointers filled in
 */
int SV_AreaEdicts (vec3_t mins, vec3_t maxs, edict_t **list, int maxCount, int areaType)
{
	areaMins = mins;
	areaMaxs = maxs;
	areaEdictList = list;
	areaEdictListCount = 0;
	areaEdictListMaxCount = maxCount;

	SV_AreaEdicts_r(sv_areaNodes, areaType);

	return areaEdictListCount;
}


/** @brief Server side moveclip - see cmodel.c */
typedef struct {
	vec3_t boxmins, boxmaxs;	/**< enclose the test object along entire move */
	const float *mins, *maxs;	/**< size of the moving object */
	float *start, *end;
	trace_t trace;
	edict_t *passedict;
	int contentmask;
} moveclip_t;


/**
 * @brief Returns a headnode that can be used for testing or clipping an
 * object of mins/maxs size.
 * Offset is filled in to contain the adjustment that must be added to the
 * testing object's origin to get a point to use with the returned hull.
 * @param[in] ent The edict to get the bmodel from (at least in case of SOLID_BSP)
 * @param[out] tile The maptile the bmodel belongs, too (at least in case of SOLID_BSP)
 */
static int SV_HullForEntity (const edict_t *ent, int *tile)
{
	assert(ent->solid != SOLID_NOT);
	assert(ent->solid != SOLID_TRIGGER);

	/* decide which clipping hull to use, based on the size */
	if (ent->solid == SOLID_BSP) {	/* explicit hulls in the BSP model */
		const cBspModel_t *model;

		assert(ent->modelindex < MAX_MODELS);

		model = sv.models[ent->modelindex];
		if (!model)
			Com_Error(ERR_FATAL, "SOLID_BSP with a non bsp model");

		*tile = model->tile;
		assert(model->headnode < MAX_MAP_NODES);
		return model->headnode;
	}

	/* create a temp hull from bounding box sizes */
	*tile = 0;
	return CM_HeadnodeForBox(0, ent->mins, ent->maxs);
}


/**
 * @sa SV_Trace
 * @sa SV_AreaEdicts
 * @sa CL_ClipMoveToLEs
 */
static void SV_ClipMoveToEntities (moveclip_t *clip)
{
	int i;
	edict_t *touchlist[MAX_EDICTS];
	trace_t trace;
	const float *angles;
	int tile = 0, headnode = 0;

	const int num = SV_AreaEdicts(clip->boxmins, clip->boxmaxs, touchlist, MAX_EDICTS, AREA_SOLID);

	/* be careful, it is possible to have an entity in this
	 * list removed before we get to it (killtriggered) */
	for (i = 0; i < num; i++) {
		edict_t *touch = touchlist[i];
		if (touch->solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;

		if (clip->trace.allsolid)
			return;

		if (clip->passedict) {
			if (touch->owner == clip->passedict)
				continue;		/* don't clip against own missiles */
			if (clip->passedict->owner == touch)
				continue;		/* don't clip against owner */
		}

		/* might intersect, so do an exact clip */
		headnode = SV_HullForEntity(touch, &tile);
		if (headnode >= MAX_MAP_NODES)
			continue;

		if (touch->solid != SOLID_BSP)
			angles = vec3_origin;	/* boxes don't rotate */
		else
			angles = touch->angles;

		assert(headnode < MAX_MAP_NODES);
		trace = CM_TransformedBoxTrace(tile, clip->start, clip->end, clip->mins, clip->maxs, headnode, clip->contentmask, 0, touch->origin, angles);

#ifdef PARANOID
		Com_DPrintf(DEBUG_SERVER, "SV_ClipMoveToEntities: %i %i: (%i %i %i) (%i %i %i) (%i %i %i)\n", touch->number, touch->modelindex,
			(int)touch->mins[0], (int)touch->mins[1], (int)touch->mins[2],
			(int)touch->maxs[0], (int)touch->maxs[1], (int)touch->maxs[2],
			(int)touch->origin[0], (int)touch->origin[1], (int)touch->origin[2]);
#endif

		if (trace.fraction < clip->trace.fraction) {
			qboolean oldStart;

			/* make sure we keep a startsolid from a previous trace */
			oldStart = clip->trace.startsolid;
			trace.ent = touch;
			clip->trace = trace;
			clip->trace.startsolid |= oldStart;
		} else if (trace.allsolid) {
			trace.ent = touch;
			clip->trace = trace;
		} else if (trace.startsolid) {
			trace.ent = touch;
			clip->trace.startsolid = qtrue;
		}
	}
}

/**
 * @brief Returns the content flags for a given point
 * @note Useful to determine whether an actor is e.g. inside of a water brush
 * @sa CM_TestInLeaf
 * @sa CM_TestBoxInBrush
 * @sa TR_CompleteBoxTrace
 */
int SV_PointContents (vec3_t p)
{
	/* clip to all world levels */
	trace_t trace = TR_CompleteBoxTrace(p, p, vec3_origin, vec3_origin, TRACING_ALL_VISIBLE_LEVELS, MASK_ALL, 0);
	if (trace.fraction == 0)
		return trace.contentFlags;		/* blocked by the world */
	return 0;
}

/**
 * @param[in] mins and maxs are relative
 * @sa SV_Trace
 */
static void SV_TraceBounds (const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
#if 0
	/* debug to test against everything */
	boxmins[0] = boxmins[1] = boxmins[2] = -9999;
	boxmaxs[0] = boxmaxs[1] = boxmaxs[2] = 9999;
#else
	int i;

	for (i = 0; i < 3; i++) {
		if (end[i] > start[i]) {
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		} else {
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
#endif
}

/**
 * @brief Moves the given mins/maxs volume through the world from start to end.
 * @note Passedict and edicts owned by passedict are explicitly not checked.
 * @sa SV_TraceBounds
 * @sa CL_Trace
 * @param[in] start
 * @param[in] end
 * @param[in] passedict is explicitly excluded from clipping checks (normally NULL)
 * if the entire move stays in a solid volume, trace.allsolid will be set,
 * trace.startsolid will be set, and trace.fraction will be 0
 * if the starting point is in a solid, it will be allowed to move out to an open area
 * @param[in] contentmask brushes the trace should stop at (see MASK_*)
 * @param[in] mins
 * @param[in] maxs
 */
trace_t SV_Trace (vec3_t start, const vec3_t mins, const vec3_t maxs, vec3_t end, edict_t * passedict, int contentmask)
{
	moveclip_t clip;

	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	memset(&clip, 0, sizeof(clip));

	/* clip to world - 0x1FF = all levels */
	clip.trace = TR_CompleteBoxTrace(start, end, mins, maxs, TRACING_ALL_VISIBLE_LEVELS, contentmask, 0);
	/** @todo There is more than one world in case of a map assembly - use
	 * @c clip.trace.mapTile to get the correct one */
	clip.trace.ent = ge->edicts; /* g_edicts[0] is the world */
	if (clip.trace.fraction == 0)
		return clip.trace;		/* blocked by the world */

	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passedict = passedict;

	/* create the bounding box for the entire path traveled by the shot */
	SV_TraceBounds(start, clip.mins, clip.maxs, end, clip.boxmins, clip.boxmaxs);

#if 0
	/* Output the trace bounds */
	Com_Printf("Trace: (%i, %i, %i) (%i, %i, %i)\n",
		(int) clip.boxmins[0], (int) clip.boxmins[1], (int) clip.boxmins[2],
		(int) clip.boxmaxs[0], (int) clip.boxmaxs[1], (int) clip.boxmaxs[2]);
#endif

	/* clip to other solid entities */
	SV_ClipMoveToEntities(&clip);

	return clip.trace;
}

/**
 * @brief
 * @sa Com_GetTerrainType
 * @sa GenerateFootstepList
 */
const char *SV_GetFootstepSound (const char *texture)
{
	const terrainType_t *t = Com_GetTerrainType(texture);
	return t ? t->footStepSound : NULL;
}

/**
 * @brief Different terrain types might have different bounce fraction
 * @sa Com_GetTerrainType
 * @sa GenerateFootstepList
 */
float SV_GetBounceFraction (const char *texture)
{
	const terrainType_t *t = Com_GetTerrainType(texture);
	return t ? t->bounceFraction : 1.0f;
}

/**
 * @brief Loads the mins/maxs for a md2 mesh model
 * @param[in,out] mod The server side model struct to store the results in
 * @param[in] buffer The mesh model buffer
 * @param[in] bufferLength The mesh model buffer length
 */
static void SV_ModLoadAliasMD2Model (sv_model_t* mod, const byte *buffer, int bufferLength)
{
	const dMD2Model_t *md2 = (const dMD2Model_t *)buffer;
	const int num_frames = LittleLong(md2->num_frames);
	const int frameSize = LittleLong(md2->framesize);
	const dMD2Frame_t *frame = (const dMD2Frame_t *) ((const byte *) md2 + LittleLong(md2->ofs_frames) + mod->frame * frameSize);
	vec3_t scale, mins, maxs;
	int j;

	if (mod->frame > num_frames)
		return;

	for (j = 0; j < 3; j++) {
		scale[j] = LittleFloat(frame->scale[j]);
		mins[j] = LittleFloat(frame->translate[j]);
	}

	VectorMA(mins, 255, scale, maxs);
	AddPointToBounds(mins, mod->mins, mod->maxs);
	AddPointToBounds(maxs, mod->mins, mod->maxs);
}

/**
 * @brief Loads the mins/maxs for a md3 mesh model
 * @param[in,out] mod The server side model struct to store the results in
 * @param[in] buffer The mesh model buffer
 * @param[in] bufferLength The mesh model buffer length
 */
static void SV_ModLoadAliasMD3Model (sv_model_t* mod, const byte *buffer, int bufferLength)
{
	const dmd3_t *md3 = (const dmd3_t *)buffer;
	const dmd3frame_t *frame = (const dmd3frame_t *)((const byte *)md3 + LittleLong(md3->ofs_frames));
	const int num_frames = LittleLong(md3->num_frames);
	vec3_t mins, maxs;
	int j;

	if (mod->frame > num_frames)
		return;

	frame += mod->frame;
	for (j = 0; j < 3; j++) {
		mins[j] = LittleFloat(frame->mins[j]);
		maxs[j] = LittleFloat(frame->maxs[j]);
	}
	AddPointToBounds(mins, mod->mins, mod->maxs);
	AddPointToBounds(maxs, mod->mins, mod->maxs);
}

/**
 * @brief Loads the mins/maxs for a dpm mesh model
 * @param[in,out] mod The server side model struct to store the results in
 * @param[in] buffer The mesh model buffer
 * @param[in] bufferLength The mesh model buffer length
 */
static void SV_ModLoadAliasDPMModel (sv_model_t* mod, const byte *buffer, int bufferLength)
{
	const dpmheader_t *dpm = (const dpmheader_t *)buffer;
	const int num_frames = BigLong(dpm->num_frames);
	const int ofs_frames = BigLong(dpm->ofs_frames);
	const dpmframe_t *frame = (const dpmframe_t *)((const byte *)dpm + ofs_frames);

	if (mod->frame > num_frames)
		return;

	frame += mod->frame;

	mod->mins[0] = BigFloat(frame->mins[0]);
	mod->mins[1] = BigFloat(frame->mins[1]);
	mod->mins[2] = BigFloat(frame->mins[2]);
	mod->maxs[0] = BigFloat(frame->maxs[0]);
	mod->maxs[1] = BigFloat(frame->maxs[1]);
	mod->maxs[2] = BigFloat(frame->maxs[2]);
}

/**
 * @brief Loads the mins/maxs for a obj mesh model
 * @param[in,out] mod The server side model struct to store the results in
 * @param[in] buffer The mesh model buffer
 * @param[in] bufferLength The mesh model buffer length
 */
static void SV_ModLoadObjModel (sv_model_t* mod, const byte *buffer, int bufferLength)
{
	/** @todo implement me */
}

/**
 * @brief Load the mins, maxs for the model on the serverside for pathfinding and clipping
 * @param[in] model The relative model path to load the mins, maxs for
 * @param[out] mins The mins vector of the model - this is absolute to the worldorigin (0,0,0)
 * @param[out] maxs The maxs vector of the model - this is absolute to the worldorigin (0,0,0)
 */
qboolean SV_LoadModelMinsMaxs (const char *model, int frame, vec3_t mins, vec3_t maxs)
{
	sv_model_t *mod;
	byte *buf;
	int i;
	int modfilelen;

	if (model[0] == '\0')
		Com_Error(ERR_DROP, "SV_LoadModelMinsMaxs: NULL model");

	/* search the currently loaded models */
	for (i = 0, mod = sv_models; i < sv_numModels; i++, mod++)
		if (mod->frame == frame && !strcmp(mod->name, model)) {
			VectorCopy(mod->mins, mins);
			VectorCopy(mod->maxs, maxs);
			return qtrue;
		}

	/* find a free model slot spot */
	for (i = 0, mod = sv_models; i < sv_numModels; i++, mod++) {
		if (!mod->name)
			break;				/* free spot */
	}

	if (i == sv_numModels) {
		if (sv_numModels == MAX_MOD_KNOWN)
			Com_Error(ERR_DROP, "sv_numModels == MAX_MOD_KNOWN");
		sv_numModels++;
	}

	memset(mod, 0, sizeof(*mod));
	mod->name = Mem_PoolStrDup(model, com_genericPool, 0);
	mod->frame = frame;

	VectorCopy(vec3_origin, mins);
	VectorCopy(vec3_origin, maxs);

	/* load the file */
	modfilelen = FS_LoadFile(model, &buf);
	if (!buf) {
		memset(mod->name, 0, sizeof(mod->name));
		sv_numModels--;
		return qfalse;
	}

	ClearBounds(mod->mins, mod->maxs);

	/* call the appropriate loader */
	switch (LittleLong(*(unsigned *) buf)) {
	case IDALIASHEADER:
		SV_ModLoadAliasMD2Model(mod, buf, modfilelen);
		break;

	case DPMHEADER:
		SV_ModLoadAliasDPMModel(mod, buf, modfilelen);
		break;

	case IDMD3HEADER:
		SV_ModLoadAliasMD3Model(mod, buf, modfilelen);
		break;

	default:
		if (!Q_strcasecmp(mod->name + strlen(mod->name) - 4, ".obj"))
			SV_ModLoadObjModel(mod, buf, modfilelen);
		else {
			FS_FreeFile(buf);
			return qfalse;
		}
	}

	VectorCopy(mod->mins, mins);
	VectorCopy(mod->maxs, maxs);

	FS_FreeFile(buf);
	return qtrue;
}
