/**
 * @file sv_world.c
 * @brief World query functions.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

26/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.

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

/*
===============================================================================
ENTITY AREA CHECKING
FIXME: this use of "area" is different from the bsp file use
===============================================================================
*/

/* (type *)STRUCT_FROM_LINK(link_t *link, type, member) */
/* ent = STRUCT_FROM_LINK(link,entity_t,order) */
/* FIXME: remove this mess! */
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (size_t)&(((t *)0)->m)))

#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edict_t,area)

typedef struct areanode_s {
	int axis;					/* -1 = leaf node */
	float dist;
	struct areanode_s *children[2];
	link_t trigger_edicts;
	link_t solid_edicts;
} areanode_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	32

static areanode_t sv_areanodes[AREA_NODES];
static int sv_numareanodes;

static float *area_mins, *area_maxs;
static edict_t **area_list;
static int area_count, area_maxcount;
static int area_type;

int SV_HullForEntity(edict_t * ent, int *tile);


/**
 * @brief ClearLink is used for new headnodes
 */
void ClearLink(link_t * l)
{
	l->prev = l->next = l;
}

/**
 * @brief
 */
void RemoveLink(link_t * l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

/**
 * @brief
 */
void InsertLinkBefore(link_t * l, link_t * before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

/**
 * @brief Builds a uniformly subdivided tree for the given world size
 * @sa SV_ClearWorld
 */
areanode_t *SV_CreateAreaNode(int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t *anode;
	vec3_t size;
	vec3_t mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink(&anode->trigger_edicts);
	ClearLink(&anode->solid_edicts);

	if (depth == AREA_DEPTH) {
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract(maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
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
void SV_ClearWorld(void)
{
	memset(sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;
	SV_CreateAreaNode(0, map_min, map_max);
}


/**
 * @brief call before removing an entity, and before trying to move one, so it doesn't clip against itself
 */
void SV_UnlinkEdict(edict_t * ent)
{
	if (!ent->area.prev)
		return;					/* not linked in anywhere */
	RemoveLink(&ent->area);
	ent->area.prev = ent->area.next = NULL;
}


#define MAX_TOTAL_ENT_LEAFS		128
/**
 * @brief Needs to be called any time an entity changes origin, mins, maxs,
 * or solid.  Automatically unlinks if needed.
 * sets ent->v.absmin and ent->v.absmax
 * sets ent->leafnums[] for pvs determination even if the entity
 * is not solid
 */
void SV_LinkEdict(edict_t * ent)
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

	/* set the abs box */
	/* normal */
	VectorAdd(ent->origin, ent->mins, ent->absmin);
	VectorAdd(ent->origin, ent->maxs, ent->absmax);

	/*get all leafs, including solids */
/*	num_leafs = CM_BoxLeafnums (ent->absmin, ent->absmax, */
/*		leafs, MAX_TOTAL_ENT_LEAFS, &topnode); */

	ent->linkcount++;

	if (ent->solid == SOLID_NOT)
		return;

	/* find the first node that the ent's box crosses */
	node = sv_areanodes;
	while (1) {
		if (node->axis == -1)
			break;
		if (ent->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;				/* crosses the node */
	}

	/* link it in    */
/*	if (ent->solid == SOLID_TRIGGER) */
/*		InsertLinkBefore (&ent->area, &node->trigger_edicts); */
/*	else */
	InsertLinkBefore(&ent->area, &node->solid_edicts);
}


/**
 * @brief fills in a table of edict pointers with edicts that have bounding boxes
 * that intersect the given area.  It is possible for a non-axial bmodel
 * to be returned that doesn't actually intersect the area on an exact test.
 * @return the number of pointers filled in
 * @note ??? does this always return the world?
 * @sa SV_AreaEdicts
 */
void SV_AreaEdicts_r(areanode_t * node)
{
	link_t *l, *next, *start;
	edict_t *check;
	int count;

	count = 0;

	/* touch linked edicts */
	if (area_type == AREA_SOLID)
		start = &node->solid_edicts;
	else
		start = &node->trigger_edicts;

	for (l = start->next; l != start; l = next) {
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->solid == SOLID_NOT)
			continue;			/* deactivated */
		if (check->absmin[0] > area_maxs[0]
			|| check->absmin[1] > area_maxs[1]
			|| check->absmin[2] > area_maxs[2]
			|| check->absmax[0] < area_mins[0]
			|| check->absmax[1] < area_mins[1]
			|| check->absmax[2] < area_mins[2])
			continue;			/* not touching */

		if (area_count == area_maxcount) {
			Com_Printf("SV_AreaEdicts: MAXCOUNT\n");
			return;
		}

		area_list[area_count] = check;
		area_count++;
	}

	if (node->axis == -1)
		return;					/* terminal node */

	/* recurse down both sides */
	if (area_maxs[node->axis] > node->dist)
		SV_AreaEdicts_r(node->children[0]);
	if (area_mins[node->axis] < node->dist)
		SV_AreaEdicts_r(node->children[1]);
}

/**
 * @brief
 * @sa SV_AreaEdicts_r
 */
int SV_AreaEdicts(vec3_t mins, vec3_t maxs, edict_t ** list, int maxcount, int areatype)
{
	area_mins = mins;
	area_maxs = maxs;
	area_list = list;
	area_count = 0;
	area_maxcount = maxcount;
	area_type = areatype;

	SV_AreaEdicts_r(sv_areanodes);

	return area_count;
}

/*=========================================================================== */

typedef struct {
	vec3_t boxmins, boxmaxs;	/* enclose the test object along entire move */
	float *mins, *maxs;			/* size of the moving object */
	vec3_t mins2, maxs2;		/* size when clipping against mosnters */
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
 */
int SV_HullForEntity(edict_t * ent, int *tile)
{
	cmodel_t *model;

	/* decide which clipping hull to use, based on the size */
	if (ent->solid == SOLID_BSP) {	/* explicit hulls in the BSP model */
		model = sv.models[ent->modelindex];

		if (!model)
			Com_Error(ERR_FATAL, "MOVETYPE_PUSH with a non bsp model");

		*tile = model->tile;
		return model->headnode;
	}

	/* create a temp hull from bounding box sizes */
	*tile = 0;
	return CM_HeadnodeForBox(0, ent->mins, ent->maxs);
}


/**
 * @brief
 */
void SV_ClipMoveToEntities(moveclip_t * clip)
{
	int i, num;
	edict_t *touchlist[MAX_EDICTS], *touch;
	trace_t trace;
	int tile = 0, headnode = 0;

	num = SV_AreaEdicts(clip->boxmins, clip->boxmaxs, touchlist, MAX_EDICTS, AREA_SOLID);

	/* be careful, it is possible to have an entity in this */
	/* list removed before we get to it (killtriggered) */
	for (i = 0; i < num; i++) {
		touch = touchlist[i];
		if (touch->solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (clip->trace.allsolid)
			return;
#if 0
		if (clip->passedict) {
			if (touch->owner == clip->passedict)
				continue;		/* don't clip against own missiles */
			if (clip->passedict->owner == touch)
				continue;		/* don't clip against owner */
		}
#endif
		/* might intersect, so do an exact clip */
		headnode = SV_HullForEntity(touch, &tile);

		trace = CM_TransformedBoxTrace(clip->start, clip->end, clip->mins, clip->maxs, tile, headnode, clip->contentmask, touch->origin, vec3_origin);

/*		Com_Printf( "%i %i: (%i %i %i) (%i %i %i) (%i %i %i)\n", touch->number, touch->modelindex,
			(int)touch->mins[0], (int)touch->mins[1], (int)touch->mins[2],
			(int)touch->maxs[0], (int)touch->maxs[1], (int)touch->maxs[2],
			(int)touch->origin[0], (int)touch->origin[1], (int)touch->origin[2] );
*/
		if (trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction) {
			trace.ent = touch;
			clip->trace = trace;
			if (clip->trace.startsolid)
				clip->trace.startsolid = qtrue;
		} else if (trace.startsolid)
			clip->trace.startsolid = qtrue;
	}
}


/**
 * @brief
 * @param[in] mins and maxs are relative
 * @param[in] passedict is explicitly excluded from clipping checks (normally NULL)
 * if the entire move stays in a solid volume, trace.allsolid will be set,
 * trace.startsolid will be set, and trace.fraction will be 0
 * if the starting point is in a solid, it will be allowed to move out to an open area
 * @sa SV_Trace
 */
void SV_TraceBounds(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
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
 */
trace_t SV_Trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t * passedict, int contentmask)
{
	moveclip_t clip;

	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	memset(&clip, 0, sizeof(moveclip_t));

	/* clip to world */
	clip.trace = CM_CompleteBoxTrace(start, end, mins, maxs, 0x1FF, contentmask);
	clip.trace.ent = ge->edicts;
	if (clip.trace.fraction == 0)
		return clip.trace;		/* blocked by the world */

	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passedict = passedict;

	FastVectorCopy(*mins, clip.mins2);
	FastVectorCopy(*maxs, clip.maxs2);

	/* create the bounding box of the entire move */
	SV_TraceBounds(start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs);

	/* clip to other solid entities */
	SV_ClipMoveToEntities(&clip);

	return clip.trace;
}
