/**
 * @file
 * @brief World query functions.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../shared/moveclip.h"
#include "server.h"
#include "../common/qfiles.h"

#define	AREA_DEPTH	4

/**
 * @brief Builds a uniformly subdivided tree for the given world size
 * @sa SV_ClearWorld
 * @sa SV_LinkEdict
 */
static worldSector_t* SV_CreateWorldSector (int depth, const AABB& sBox)
{
	if (sv->numWorldSectors >= lengthof(sv->worldSectors))
		Com_Error(ERR_DROP, "SV_CreateWorldSector: overflow");

	worldSector_t* anode = &sv->worldSectors[sv->numWorldSectors];
	sv->numWorldSectors++;

	anode->entities = nullptr;

	if (depth == AREA_DEPTH) {
		anode->axis = LEAFNODE; /* end of tree */
		anode->children[0] = anode->children[1] = nullptr;
		return anode;
	}

	vec3_t size;
	sBox.getDiagonal(size);
	if (size[0] > size[1])
		anode->axis = PLANE_X;
	else
		anode->axis = PLANE_Y;

	anode->dist = 0.5f * (sBox.maxs[anode->axis] + sBox.mins[anode->axis]);
	AABB sBox1(sBox);
	AABB sBox2(sBox);

	sBox1.maxs[anode->axis] = sBox2.mins[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateWorldSector(depth + 1, sBox2);
	anode->children[1] = SV_CreateWorldSector(depth + 1, sBox1);

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
	SV_CreateWorldSector(0, sv->mapData.mapBox);
}

static inline sv_edict_t* SV_GetServerDataForEdict (const edict_t* ent)
{
	if (!ent || ent->number < 0 || ent->number >= lengthof(sv->edicts))
		Com_Error(ERR_DROP, "SV_GetServerDataForEdict: bad game ent");

	return &sv->edicts[ent->number];
}

/**
 * @brief call before removing an entity, and before trying to move one, so it doesn't clip against itself
 */
void SV_UnlinkEdict (edict_t* ent)
{
	sv_edict_t* sv_ent = SV_GetServerDataForEdict(ent);
	sv_edict_t* scan;
	worldSector_t* ws;

	sv_ent->linked = false;

	ws = sv_ent->worldSector;
	if (!ws)
		return;					/* not linked in anywhere */

	sv_ent->worldSector = nullptr;

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
 * or solid. Automatically unlinks if needed. Sets ent->absmin and ent->absBox.maxs
 * @sa SV_CreateAreaNode
 */
void SV_LinkEdict (edict_t* ent)
{
	worldSector_t* node;
	sv_edict_t* sv_ent = SV_GetServerDataForEdict(ent);

	if (sv_ent->worldSector)
		SV_UnlinkEdict(ent);	/* unlink from old position */

	if (ent == svs.ge->edicts)
		return;					/* don't add the world */

	if (!ent->inuse)
		return;

	/* set the size */
	ent->entBox.getDiagonal(ent->size);

	/* increase the linkcount - even for none solids */
	ent->linkcount++;

	CalculateMinsMaxs(ent->solid == SOLID_BSP ? ent->angles : vec3_origin, ent->entBox, ent->origin, ent->absBox);

	/* if not solid we have to set the abs mins/maxs above but don't really link it */
	if (ent->solid == SOLID_NOT)
		return;

	/* find the first node that the ent's box crosses */
	node = sv->worldSectors;
	while (1) {
		/* end of tree */
		if (node->axis == LEAFNODE)
			break;
		if (ent->absBox.mins[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->absBox.maxs[node->axis] < node->dist)
			node = node->children[1];
		else
			break;				/* crosses the node */
	}

	/* link it in */
	sv_ent->nextEntityInWorldSector = node->entities;
	node->entities = sv_ent;

	sv_ent->linked = true;
	sv_ent->worldSector = node;
	sv_ent->ent = ent;

	/* If this ent has a child, link it back in, too */
	if (ent->child()) {
		ent->child()->entBox.set(ent->absBox);

		/* expand the trigger box */
		ent->child()->entBox.expandXY(UNIT_SIZE / 2);
		/* link child back into the world */
		SV_LinkEdict(ent->child());
	}
}

/**
 * @brief Checks whether the bounding box of the given edict will intersect with the given bbox
 * @param[in] aabb the bounding box
 * @param[in] ent The edict to check the intersection for
 * @return @c true if intersect, @c false otherwise
 */
static bool SV_BoundingBoxesIntersect (const AABB& aabb, const edict_t* ent)
{
	return aabb.doesIntersect(ent->absBox);
}

typedef struct {
	AABB areaBox;
	edict_t** areaEdictList;
	int areaEdictListCount, areaEdictListMaxCount;
} areaParms_t;

/**
 * @brief fills in a table of edict pointers with edicts that have bounding boxes
 * that intersect the given area. It is possible for a non-axial bmodel
 * to be returned that doesn't actually intersect the area on an exact test.
 * @sa SV_AreaEdicts
 */
static void SV_AreaEdicts_r (worldSector_t* node, areaParms_t* ap)
{
	sv_edict_t* check, *next;

	for (check = node->entities; check; check = next) {
		next = check->nextEntityInWorldSector;

		/* deactivated */
		if (check->ent->solid == SOLID_NOT)
			continue;

		if (!check->ent->inuse)
			continue;

		if (!SV_BoundingBoxesIntersect(ap->areaBox, check->ent))
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
	if (ap->areaBox.maxs[node->axis] > node->dist)
		SV_AreaEdicts_r(node->children[0], ap);
	if (ap->areaBox.mins[node->axis] < node->dist)
		SV_AreaEdicts_r(node->children[1], ap);
}

/**
 * @sa SV_AreaEdicts_r
 * @param[in] abox The bounding box
 * @param[out] list The edict list that this trace is hitting
 * @param[in] maxCount The size of the given @c list
 * @return the number of pointers filled in
 */
static int SV_AreaEdicts (const AABB& abox, edict_t** list, int maxCount)
{
	areaParms_t	ap;

	ap.areaBox.set(abox);
	ap.areaEdictList = list;
	ap.areaEdictListCount = 0;
	ap.areaEdictListMaxCount = maxCount;

	SV_AreaEdicts_r(sv->worldSectors, &ap);

	return ap.areaEdictListCount;
}

/** @brief Server side moveclip - see cmodel.c */
class MoveClipSV : public MoveClip
{
public:
	trace_t trace;
	const edict_t* passedict;
};


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
static int SV_HullForEntity (const edict_t* ent, int* tile, vec3_t rmaShift)
{
	assert(ent->solid != SOLID_NOT);
	assert(ent->solid != SOLID_TRIGGER);

	/* decide which clipping hull to use, based on the size */
	if (ent->solid == SOLID_BSP) {	/* explicit hulls in the BSP model */
		const cBspModel_t* model;

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
	return CM_HeadnodeForBox(sv->mapTiles.mapTiles[*tile], ent->entBox);
}


/**
 * @sa SV_Trace
 * @sa SV_AreaEdicts
 * @sa CL_ClipMoveToLEs
 */
static void SV_ClipMoveToEntities (MoveClipSV* clip)
{
	edict_t* touchlist[MAX_EDICTS];
	const int num = SV_AreaEdicts(clip->clipBox, touchlist, MAX_EDICTS);

	/* be careful, it is possible to have an entity in this
	 * list removed before we get to it (killtriggered) */
	for (int i = 0; i < num; i++) {
		vec3_t rmaShift;
		edict_t* touch = touchlist[i];
		int tile = 0;

		if (touch->solid == SOLID_NOT || touch->solid == SOLID_TRIGGER)
			continue;
		if (touch == clip->passedict)
			continue;

		if (clip->trace.allsolid)
			return;

		if (clip->passedict) {
			if (touch->isParentship(clip->passedict))	/* check if one of them is the owner of the other */
				continue;		/* don't clip against own missiles or owner */
		}

		/* might intersect, so do an exact clip */
		int headnode = SV_HullForEntity(touch, &tile, rmaShift);
		if (headnode >= MAX_MAP_NODES)
			continue;

		const float* angles;
		if (touch->solid != SOLID_BSP)
			angles = vec3_origin;	/* boxes don't rotate */
		else
			angles = touch->angles;

		assert(headnode < MAX_MAP_NODES);
		trace_t trace = CM_HintedTransformedBoxTrace(sv->mapTiles.mapTiles[tile], clip->moveLine, clip->objBox, headnode,
				clip->contentmask, 0, touch->origin, angles, rmaShift, 1.0);

#ifdef PARANOID
		Com_DPrintf(DEBUG_SERVER, "SV_ClipMoveToEntities: %i %i: (%i %i %i) (%i %i %i) (%i %i %i)\n", touch->number, touch->modelindex,
			(int)touch->entBox.mins[0], (int)touch->entBox.mins[1], (int)touch->entBox.mins[2],
			(int)touch->entBox.maxs[0], (int)touch->entBox.maxs[1], (int)touch->entBox.maxs[2],
			(int)touch->origin[0], (int)touch->origin[1], (int)touch->origin[2]);
#endif

		if (trace.fraction < clip->trace.fraction) {
			bool oldStart;

			/* make sure we keep a startsolid from a previous trace */
			oldStart = clip->trace.startsolid;
			trace.entNum = touch->number;
			clip->trace = trace;
			clip->trace.startsolid |= oldStart;
		} else if (trace.allsolid) {
			trace.entNum = touch->number;
			clip->trace = trace;
		} else if (trace.startsolid) {
			trace.entNum = touch->number;
			clip->trace.startsolid = true;
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
int SV_PointContents (const vec3_t p)
{
	/* clip to all world levels */
	const trace_t trace = CM_CompleteBoxTrace(&sv->mapTiles, Line(p, p), AABB::EMPTY, TRACE_ALL_LEVELS, MASK_ALL, 0);
	if (trace.fraction == 0)
		return trace.contentFlags;		/* blocked by the world */
	return 0;
}

/**
 * @brief Moves the given mins/maxs volume through the world from start to end.
 * @note Passedict and edicts owned by passedict are explicitly not checked.
 * @sa SV_TraceBounds
 * @sa CL_Trace
 * @param[in] traceLine The from/to position in the world for this trace
 * @param[in] box The bounding box that is moved through the world
 * @param[in] passedict is explicitly excluded from clipping checks (normally nullptr)
 * if the entire move stays in a solid volume, trace.allsolid will be set,
 * trace.startsolid will be set, and trace.fraction will be 0
 * if the starting point is in a solid, it will be allowed to move out to an open area
 * @param[in] contentmask brushes the trace should stop at (see MASK_*)
 */
trace_t SV_Trace (const Line& traceLine, const AABB& box, const edict_t* passedict, int contentmask)
{
	MoveClipSV clip;

	OBJZERO(clip);

	/* clip to world - 0x1FF = all levels */
	clip.trace = CM_CompleteBoxTrace(&sv->mapTiles, traceLine, box, TRACE_ALL_LEVELS, contentmask, 0);
	/** @todo There is more than one world in case of a map assembly - use
	 * @c clip.trace.mapTile to get the correct one */
	clip.trace.entNum = 0; /* the first edict is the world */
	if (clip.trace.fraction == 0)
		return clip.trace;		/* blocked by the world */

	clip.contentmask = contentmask;
	clip.moveLine.set(traceLine);
	clip.objBox.set(box);
	clip.passedict = passedict;

	/* create the bounding box for the entire path traveled by the shot */
	clip.calcBounds();

	/* clip to other solid entities */
	SV_ClipMoveToEntities(&clip);

	return clip.trace;
}

/**
 * @brief Query the footstep sound for the given surface texture
 * @sa Com_GetTerrainType
 * @sa GenerateFootstepList
 * @return either @c nullptr or the footstep sound filename if there is one assigned in the scripts
 */
const char* SV_GetFootstepSound (const char* texture)
{
	const terrainType_t* t = Com_GetTerrainType(texture);
	return t ? t->footstepSound : nullptr;
}

/**
 * @brief Different terrain types might have different bounce fraction
 * @sa Com_GetTerrainType
 * @sa GenerateFootstepList
 */
float SV_GetBounceFraction (const char* texture)
{
	const terrainType_t* t = Com_GetTerrainType(texture);
	return t ? t->bounceFraction : 1.0f;
}

/**
 * @brief Loads the mins/maxs for a md2 mesh model
 * @param[in,out] mod The server side model struct to store the results in
 * @param[in] buffer The mesh model buffer
 */
static void SV_ModLoadAliasMD2Model (sv_model_t* mod, const byte* buffer)
{
	const dMD2Model_t* md2 = (const dMD2Model_t*)buffer;
	const int num_frames = LittleLong(md2->num_frames);
	const int frameSize = LittleLong(md2->framesize);
	const dMD2Frame_t* frame = (const dMD2Frame_t*) ((const byte*) md2 + LittleLong(md2->ofs_frames) + mod->frame * frameSize);
	vec3_t scale, mins, maxs;

	if (mod->frame > num_frames)
		return;

	for (int j = 0; j < 3; j++) {
		scale[j] = LittleFloat(frame->scale[j]);
		mins[j] = LittleFloat(frame->translate[j]);
	}

	VectorMA(mins, 255, scale, maxs);
	mod->aabb.add(mins);
	mod->aabb.add(maxs);
}

/**
 * @brief Loads the mins/maxs for a md3 mesh model
 * @param[in,out] mod The server side model struct to store the results in
 * @param[in] buffer The mesh model buffer
 */
static void SV_ModLoadAliasMD3Model (sv_model_t* mod, const byte* buffer)
{
	const dmd3_t* md3 = (const dmd3_t*)buffer;
	const dmd3frame_t* frame = (const dmd3frame_t*)((const byte*)md3 + LittleLong(md3->ofs_frames));
	const int num_frames = LittleLong(md3->num_frames);
	vec3_t mins, maxs;

	if (mod->frame > num_frames)
		return;

	frame += mod->frame;
	for (int j = 0; j < 3; j++) {
		mins[j] = LittleFloat(frame->mins[j]);
		maxs[j] = LittleFloat(frame->maxs[j]);
	}
	mod->aabb.add(mins);
	mod->aabb.add(maxs);
}

/**
 * @brief Loads the mins/maxs for a obj mesh model
 * @param[in,out] mod The server side model struct to store the results in
 * @param[in] buffer The mesh model buffer
 * @param[in] bufferLength The mesh model buffer length
 */
static void SV_ModLoadObjModel (sv_model_t* mod, const byte* buffer, int bufferLength)
{
	/** @todo implement me */
}

/**
 * @brief all supported model formats
 * @sa modtype_t
 */
static char const* const mod_extensions[] = {
	"md2", "md3", "obj", nullptr
};

/**
 * @brief Load the bounding box for the model on the serverside for pathfinding and clipping
 * @param[in] model The relative model path to load the bounding box for
 * @param[in] frame The frame to load the bounding box for
 * @param[out] aabb The bounding box of the model - this is absolute to the worldorigin (0,0,0)
 */
bool SV_LoadModelAABB (const char* model, int frame, AABB& aabb)
{
	sv_model_t* mod;
	byte* buf = nullptr;
	unsigned int i;
	int modfilelen = 0;

	if (model[0] == '\0')
		Com_Error(ERR_DROP, "SV_LoadModelAABB: nullptr model");

	/* search the currently loaded models */
	for (i = 0, mod = sv->svModels; i < sv->numSVModels; i++, mod++)
		if (mod->frame == frame && Q_streq(mod->name, model)) {
			aabb.set(mod->aabb);
			return true;
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

	aabb.reset();

	/* load the file */
	if (Com_GetExtension(model) == nullptr) {
		char filename[MAX_QPATH];

		for (i = 0; mod_extensions[i] != nullptr; i++) {
			Com_sprintf(filename, sizeof(filename), "%s.%s", model, mod_extensions[i]);
			modfilelen = FS_LoadFile(filename, &buf);
			if (buf) {
				break;
			}
		}
	} else {
		modfilelen = FS_LoadFile(model, &buf);
	}

	if (!buf) {
		sv->numSVModels--;
		return false;
	}

	OBJZERO(*mod);
	mod->name = Mem_PoolStrDup(model, com_genericPool, 0);
	mod->frame = frame;
	mod->aabb.setNegativeVolume();

	/* call the appropriate loader */
	switch (LittleLong(*(unsigned* ) buf)) {
	case IDALIASHEADER:
		SV_ModLoadAliasMD2Model(mod, buf);
		break;

	case IDMD3HEADER:
		SV_ModLoadAliasMD3Model(mod, buf);
		break;

	default:
		if (!Q_strcasecmp(mod->name + strlen(mod->name) - 4, ".obj"))
			SV_ModLoadObjModel(mod, buf, modfilelen);
		else {
			FS_FreeFile(buf);
			return false;
		}
		break;
	}

	aabb.set(mod->aabb);	/* to return the found values */

	FS_FreeFile(buf);
	return true;
}
