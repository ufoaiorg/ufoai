/**
 * @file sv_world.c
 * @brief World query functions.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "../common/qfiles.h"

#define	AREA_DEPTH	4

/**
 * @brief Builds a uniformly subdivided tree for the given world size
 * @sa SV_ClearWorld
 * @sa SV_LinkEdict
 */
static worldSector_t *SV_CreateWorldSector (int depth, const vec3_t mins, const vec3_t maxs)
{
	worldSector_t *anode;
	vec3_t size;
	vec3_t mins1, maxs1, mins2, maxs2;

	if (sv->numWorldSectors >= lengthof(sv->worldSectors))
		Com_Error(ERR_DROP, "SV_CreateWorldSector: overflow");

	anode = &sv->worldSectors[sv->numWorldSectors];
	sv->numWorldSectors++;

	anode->entities = NULL;

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

	anode->children[0] = SV_CreateWorldSector(depth + 1, mins2, maxs2);
	anode->children[1] = SV_CreateWorldSector(depth + 1, mins1, maxs1);

	return anode;
}

/**
 * @brief Clear physics interaction links
 * @note Called after the world model has been loaded, before linking any entities
 * @sa SV_SpawnServer
 * @sa SV_CreateAreaNode
 */
void SV_ClearWorld (void)
{
	SV_CreateWorldSector(0, sv->mapData.mapMin, sv->mapData.mapMax);
}

static inline sv_edict_t* SV_GetServerDataForEdict (const edict_t *ent)
{
	if (!ent || ent->number < 0 || ent->number >= lengthof(sv->edicts))
		Com_Error(ERR_DROP, "SV_GetServerDataForEdict: bad game ent");

	return &sv->edicts[ent->number];
}

/**
 * @brief call before removing an entity, and before trying to move one, so it doesn't clip against itself
 */
void SV_UnlinkEdict (edict_t * ent)
{
	sv_edict_t *sv_ent = SV_GetServerDataForEdict(ent);
	sv_edict_t *scan;
	worldSector_t *ws;

	sv_ent->linked = qfalse;

	ws = sv_ent->worldSector;
	if (!ws)
		return;					/* not linked in anywhere */

	sv_ent->worldSector = NULL;

	if (ws->entities == sv_ent) {
		ws->entities = sv_ent->nextEntityInWorldSector;
		return;
	}

	for (scan = ws->entities; scan; scan = scan->nextEntityInWorldSector) {
		if (scan->nextEntityInWorldSector == sv_ent) {
			scan->nextEntityInWorldSector = sv_ent->nextEntityInWorldSector;
			return;
		}
	}

	Com_Printf("WARNING: SV_UnlinkEntity: not found in worldSector\n");
}

/**
 * @brief Needs to be called any time an entity changes origin, mins, maxs,
 * or solid. Automatically unlinks if needed. Sets ent->absmin and ent->absmax
 * @sa SV_CreateAreaNode
 */
void SV_LinkEdict (edict_t * ent)
{
	worldSector_t *node;
	sv_edict_t *sv_ent = SV_GetServerDataForEdict(ent);

	if (sv_ent->worldSector)
		SV_UnlinkEdict(ent);	/* unlink from old position */

	if (ent == svs.ge->edicts)
		return;					/* don't add the world */

	if (!ent->inuse)
		return;

	/* set the size */
	VectorSubtract(ent->maxs, ent->mins, ent->size);

	/* increase the linkcount - even for none solids */
	ent->linkcount++;

	CalculateMinsMaxs(ent->solid == SOLID_BSP ? ent->angles : vec3_origin, ent->mins, ent->maxs, ent->origin, ent->absmin, ent->absmax);

	/* if not solid we have to set the abs mins/maxs above but don't really link it */
	if (ent->solid == SOLID_NOT)
		return;

	/* find the first node that the ent's box crosses */
	node = sv->worldSectors;
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
	sv_ent->nextEntityInWorldSector = node->entities;
	node->entities = sv_ent;

	sv_ent->linked = qtrue;
	sv_ent->worldSector = node;
	sv_ent->ent = ent;

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
 * @brief Checks whether the bounding box of the given edict will intersect with the given bbox
 * @param mins The mins of the bounding box
 * @param maxs The maxs of the bounding box
 * @param ent The edict to check the intersection for
 * @return @c true if intersect, @c false otherwise
 */
static qboolean SV_BoundingBoxesIntersect (const vec3_t mins, const vec3_t maxs, const edict_t *ent)
{
	const qboolean outsideMaxs = ent->absmin[0] > maxs[0] || ent->absmin[1] > maxs[1] || ent->absmin[2] > maxs[2];
	const qboolean outsideMins = ent->absmax[0] < mins[0] || ent->absmax[1] < mins[1] || ent->absmax[2] < mins[2];
	if (outsideMaxs || outsideMins)
		return qfalse; /* not touching */
	return qtrue;
}

typedef struct {
	const float *areaMins, *areaMaxs;
	edict_t **areaEdictList;
	int areaEdictListCount, areaEdictListMaxCount;
} areaParms_t;

/**
 * @brief fills in a table of edict pointers with edicts that have bounding boxes
 * that intersect the given area. It is possible for a non-axial bmodel
 * to be returned that doesn't actually intersect the area on an exact test.
 * @sa SV_AreaEdicts
 */
static void SV_AreaEdicts_r (worldSector_t * node, areaParms_t *ap)
{
	sv_edict_t *check, *next;

	for (check = node->entities; check; check = next) {
		next = check->nextEntityInWorldSector;

		/* deactivated */
		if (check->ent->solid == SOLID_NOT)
			continue;

		if (!check->ent->inuse)
			continue;

		if (!SV_BoundingBoxesIntersect(ap->areaMins, ap->areaMaxs, check->ent))
			continue;			/* not touching */

		if (ap->areaEdictListCount == ap->areaEdictListMaxCount) {
			Com_Printf("SV_AreaEdicts_r: MAXCOUNT\n");
			return;
		}

		ap->areaEdictList[ap->areaEdictListCount] = check->ent;
		ap->areaEdictListCount++;
	}

	if (node->axis == LEAFNODE)
		return;					/* terminal node - end of tree */

	/* recurse down both sides */
	if (ap->areaMaxs[node->axis] > node->dist)
		SV_AreaEdicts_r(node->children[0], ap);
	if (ap->areaMins[node->axis] < node->dist)
		SV_AreaEdicts_r(node->children[1], ap);
}

/**
 * @sa SV_AreaEdicts_r
 * @param[in] mins The mins of the bounding box
 * @param[in] maxs The maxs of the bounding box
 * @param[out] list The edict list that this trace is hitting
 * @param[in] maxCount The size of the given @c list
 * @return the number of pointers filled in
 */
int SV_AreaEdicts (const vec3_t mins, const vec3_t maxs, edict_t **list, int maxCount)
{
	areaParms_t	ap;

	ap.areaMins = mins;
	ap.areaMaxs = maxs;
	ap.areaEdictList = list;
	ap.areaEdictListCount = 0;
	ap.areaEdictListMaxCount = maxCount;

	SV_AreaEdicts_r(sv->worldSectors, &ap);

	return ap.areaEdictListCount;
}

/**
 * @brief Fills a list with edicts that are in use and are touching the given bounding box
 * @param[in] mins The mins of the bounding box
 * @param[in] maxs The maxs of the bounding box
 * @param[out] list The edict list that this trace is hitting
 * @param[in] maxCount The size of the given @c list
 * @param[in] skip An edict to skip (e.g. pointer to the calling edict)
 * @return the number of pointers filled in
 */
int SV_TouchEdicts (const vec3_t mins, const vec3_t maxs, edict_t **list, int maxCount, edict_t *skip)
{
	int num = 0;
	const int max = min(maxCount, svs.ge->num_edicts);
	int i;

	/* skip the world */
	for (i = 1; i < max; i++) {
		edict_t *e = EDICT_NUM(i);
		/* deactivated */
		if (e->solid == SOLID_NOT)
			continue;
		if (e == skip)
			continue;
		if (SV_BoundingBoxesIntersect(mins, maxs, e))
			list[num++] = e;
	}

	return num;
}

/** @brief Server side moveclip - see cmodel.c */
typedef struct {
	vec3_t boxmins, boxmaxs;	/**< enclose the test object along entire move */
	const float *mins, *maxs;	/**< size of the moving object */
	const float *start, *end;
	trace_t trace;
	const edict_t *passedict;
	int contentmask;
} moveclip_t;


/**
 * @brief Returns a headnode that can be used for testing or clipping an
 * object of mins/maxs size.
 * Offset is filled in to contain the adjustment that must be added to the
 * testing object's origin to get a point to use with the returned hull.
 * @param[in] ent The edict to get the bmodel from (at least in case of SOLID_BSP)
 * @param[out] tile The maptile the bmodel belongs, too (at least in case of SOLID_BSP)
 * @param[out] rmaShift the shift vector in case of an RMA (needed for doors)
 * @return The headnode for the edict
 * @sa CL_HullForEntity
 */
static int SV_HullForEntity (const edict_t *ent, int *tile, vec3_t rmaShift)
{
	assert(ent->solid != SOLID_NOT);
	assert(ent->solid != SOLID_TRIGGER);

	/* decide which clipping hull to use, based on the size */
	if (ent->solid == SOLID_BSP) {	/* explicit hulls in the BSP model */
		const cBspModel_t *model;

		assert(ent->modelindex < MAX_MODELS);

		model = sv->models[ent->modelindex];
		if (!model)
			Com_Error(ERR_FATAL, "SOLID_BSP with a non bsp model");

		*tile = model->tile;
		VectorCopy(model->shift, rmaShift);
		assert(model->headnode < MAX_MAP_NODES);
		return model->headnode;
	}

	/* create a temp hull from bounding box sizes */
	*tile = 0;
	VectorCopy(vec3_origin, rmaShift);
	return CM_HeadnodeForBox(&sv->mapTiles.mapTiles[*tile], ent->mins, ent->maxs);
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
	int headnode = 0;
	const int num = SV_AreaEdicts(clip->boxmins, clip->boxmaxs, touchlist, MAX_EDICTS);

	/* be careful, it is possible to have an entity in this
	 * list removed before we get to it (killtriggered) */
	for (i = 0; i < num; i++) {
		vec3_t rmaShift;
		edict_t *touch = touchlist[i];
		int tile = 0;

		if (touch->solid == SOLID_NOT || touch->solid == SOLID_TRIGGER)
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
		headnode = SV_HullForEntity(touch, &tile, rmaShift);
		if (headnode >= MAX_MAP_NODES)
			continue;

		if (touch->solid != SOLID_BSP)
			angles = vec3_origin;	/* boxes don't rotate */
		else
			angles = touch->angles;

		assert(headnode < MAX_MAP_NODES);
		trace = CM_HintedTransformedBoxTrace(&sv->mapTiles.mapTiles[tile], clip->start, clip->end, clip->mins, clip->maxs, headnode,
				clip->contentmask, 0, touch->origin, angles, rmaShift, 1.0);

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
 * @sa CM_CompleteBoxTrace
 */
int SV_PointContents (vec3_t p)
{
	/* clip to all world levels */
	trace_t trace = CM_CompleteBoxTrace(&sv->mapTiles, p, p, vec3_origin, vec3_origin, TRACING_ALL_VISIBLE_LEVELS, MASK_ALL, 0);
	if (trace.fraction == 0)
		return trace.contentFlags;		/* blocked by the world */
	return 0;
}

/**
 * @brief calculates the bounding box for the whole trace
 * @param[in] start The starting point of the trace
 * @param[in] mins extents of the box we are moving through the world
 * @param[in] maxs guess what
 * @param[in] end The point where the trace should end
 * @param[out] boxmins The lower bounds of the trace
 * @param[out] boxmaxs The upper bounds
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
 * @param[in] start The starting position in the world for this trace
 * @param[in] end The position in the world where this trace should stop
 * @param[in] passedict is explicitly excluded from clipping checks (normally NULL)
 * if the entire move stays in a solid volume, trace.allsolid will be set,
 * trace.startsolid will be set, and trace.fraction will be 0
 * if the starting point is in a solid, it will be allowed to move out to an open area
 * @param[in] contentmask brushes the trace should stop at (see MASK_*)
 * @param[in] mins The mins of the bounding box that is moved through the world
 * @param[in] maxs The maxs of the bounding box that is moved through the world
 */
trace_t SV_Trace (const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const edict_t * passedict, int contentmask)
{
	moveclip_t clip;

	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	OBJZERO(clip);

	/* clip to world - 0x1FF = all levels */
	clip.trace = CM_CompleteBoxTrace(&sv->mapTiles, start, end, mins, maxs, TRACING_ALL_VISIBLE_LEVELS, contentmask, 0);
	/** @todo There is more than one world in case of a map assembly - use
	 * @c clip.trace.mapTile to get the correct one */
	clip.trace.ent = svs.ge->edicts; /* the first edict is the world */
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
 */
static void SV_ModLoadAliasMD2Model (sv_model_t* mod, const byte *buffer)
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
 */
static void SV_ModLoadAliasMD3Model (sv_model_t* mod, const byte *buffer)
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
 */
static void SV_ModLoadAliasDPMModel (sv_model_t* mod, const byte *buffer)
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
 * @param[in] frame The frame to load the mins and maxs vectors for
 * @param[out] mins The mins vector of the model - this is absolute to the worldorigin (0,0,0)
 * @param[out] maxs The maxs vector of the model - this is absolute to the worldorigin (0,0,0)
 */
qboolean SV_LoadModelMinsMaxs (const char *model, int frame, vec3_t mins, vec3_t maxs)
{
	sv_model_t *mod;
	byte *buf;
	unsigned int i;
	int modfilelen;

	if (model[0] == '\0')
		Com_Error(ERR_DROP, "SV_LoadModelMinsMaxs: NULL model");

	/* search the currently loaded models */
	for (i = 0, mod = sv->svModels; i < sv->numSVModels; i++, mod++)
		if (mod->frame == frame && Q_streq(mod->name, model)) {
			VectorCopy(mod->mins, mins);
			VectorCopy(mod->maxs, maxs);
			return qtrue;
		}

	/* find a free model slot spot */
	for (i = 0, mod = sv->svModels; i < sv->numSVModels; i++, mod++) {
		if (!mod->name)
			break;				/* free spot */
	}

	if (i == sv->numSVModels) {
		if (sv->numSVModels == MAX_MOD_KNOWN)
			Com_Error(ERR_DROP, "sv->numSVModels == MAX_MOD_KNOWN");
		sv->numSVModels++;
	}

	VectorCopy(vec3_origin, mins);
	VectorCopy(vec3_origin, maxs);

	/* load the file */
	modfilelen = FS_LoadFile(model, &buf);
	if (!buf) {
		sv->numSVModels--;
		return qfalse;
	}

	OBJZERO(*mod);
	mod->name = Mem_PoolStrDup(model, com_genericPool, 0);
	mod->frame = frame;
	ClearBounds(mod->mins, mod->maxs);

	/* call the appropriate loader */
	switch (LittleLong(*(unsigned *) buf)) {
	case IDALIASHEADER:
		SV_ModLoadAliasMD2Model(mod, buf);
		break;

	case DPMHEADER:
		SV_ModLoadAliasDPMModel(mod, buf);
		break;

	case IDMD3HEADER:
		SV_ModLoadAliasMD3Model(mod, buf);
		break;

	default:
		if (!Q_strcasecmp(mod->name + strlen(mod->name) - 4, ".obj"))
			SV_ModLoadObjModel(mod, buf, modfilelen);
		else {
			FS_FreeFile(buf);
			return qfalse;
		}
		break;
	}

	VectorCopy(mod->mins, mins);
	VectorCopy(mod->maxs, maxs);

	FS_FreeFile(buf);
	return qtrue;
}
