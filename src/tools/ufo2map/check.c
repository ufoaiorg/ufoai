/**
 * @file check.c
 * @brief Performs check on a loaded mapfile
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

#include "common/shared.h"
#include "common/bspfile.h"
#include "common/scriplib.h"
#include "check.h"
#include "bsp.h"

static int checkWorld (entity_t *e, int entnum)
{
	return 0;
}

static int checkLight (entity_t *e, int entnum)
{
	return 0;
}

static int checkFuncRotating (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val) {
		char buf[16];
		Com_Printf("* ERROR: func_rotating with no levelflags given - entnum: %i\n", entnum);
		snprintf(buf, sizeof(buf) - 1, "%i", (CONTENTS_LEVEL_ALL >> 8));
		SetKeyValue(e, "spawnflags", buf);
	}

	if (!e->numbrushes) {
		Com_Printf("  ERROR: func_door with no brushes given - entnum: %i\n", entnum);
		return 1;
	}
	return 0;
}

static int checkFuncDoor (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val) {
		char buf[16];
		Com_Printf("* ERROR: func_door with no levelflags given - entnum: %i\n", entnum);
		snprintf(buf, sizeof(buf) - 1, "%i", (CONTENTS_LEVEL_ALL >> 8));
		SetKeyValue(e, "spawnflags", buf);
	}

	if (!e->numbrushes) {
		Com_Printf("  ERROR: func_door with no brushes given - entnum: %i\n", entnum);
		return 1;
	}
	return 0;
}

static int checkFuncBreakable (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val) {
		char buf[16];
		Com_Printf("* ERROR: func_breakable with no levelflags given - entnum: %i\n", entnum);
		snprintf(buf, sizeof(buf) - 1, "%i", (CONTENTS_LEVEL_ALL >> 8));
		SetKeyValue(e, "spawnflags", buf);
	}

	if (!e->numbrushes) {
		Com_Printf("  ERROR: func_breakable with no brushes given - entnum: %i\n", entnum);
		return 1;
	} else if (e->numbrushes > 1) {
		Com_Printf("  ERROR: func_breakable with more than one brush given - entnum: %i (might break pathfinding)\n", entnum);
		return 1;
	}

	return 0;
}

static int checkMiscModel (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val) {
		char buf[16];
		Com_Printf("* ERROR: misc_model with no levelflags given - entnum: %i\n", entnum);
		snprintf(buf, sizeof(buf) - 1, "%i", (CONTENTS_LEVEL_ALL >> 8));
		SetKeyValue(e, "spawnflags", buf);
	}
	val = ValueForKey(e, "model");
	if (!*val) {
		Com_Printf("  ERROR: misc_model with no model given - entnum: %i\n", entnum);
		return 1;
	}

	return 0;
}

static int checkMiscParticle (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val) {
		char buf[16];
		Com_Printf("* ERROR: misc_particle with no levelflags given - entnum: %i\n", entnum);
		snprintf(buf, sizeof(buf) - 1, "%i", (CONTENTS_LEVEL_ALL >> 8));
		SetKeyValue(e, "spawnflags", buf);
	}

	val = ValueForKey(e, "particle");
	if (!*val) {
		Com_Printf("  ERROR: misc_particle with no particle given - entnum: %i\n", entnum);
		return 1;
	}

	return 0;
}

static int checkMiscMission (entity_t *e, int entnum)
{
	return 0;
}

static int checkFuncGroup (entity_t *e, int entnum)
{
	if (!e->numbrushes) {
		Com_Printf("  ERROR: func_group with no brushes given - entnum: %i\n", entnum);
		return 1;
	}
	return 0;
}

static int checkStartPosition (entity_t *e, int entnum)
{
	int align = 16;
	const char *val = ValueForKey(e, "classname");

	if (!strcmp(val, "info_2x2_start"))
		align = 32;

	if (((int)e->origin[0] - align) % UNIT_SIZE || ((int)e->origin[1] - align) % UNIT_SIZE) {
		Com_Printf("  ERROR: found misaligned starting position - entnum: %i (%i: %i)\n", entnum, (int)e->origin[0], (int)e->origin[1]);
		return 1;
	}
	return 0;
}

static int checkInfoPlayerStart (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "team");
	if (!*val) {
		Com_Printf("  ERROR: info_player_start with no team given - entnum: %i\n", entnum);
		return 1;
	}
	return checkStartPosition(e, entnum);
}

static int checkInfoNull (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "targetname");
	if (!*val) {
		Com_Printf("  ERROR: info_null with no targetname given - entnum: %i\n", entnum);
		return 1;
	}
	return 0;
}

static int checkInfoCivilianTarget (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "count");
	if (!*val)
		Com_Printf("  ERROR: info_civilian_target with no count value given - entnum: %i\n", entnum);
	return 0;
}

static int checkMiscSound (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "noise");
	if (!*val) {
		Com_Printf("  ERROR: misc_sound with no noise given - entnum: %i\n", entnum);
		return 1;
	}
	return 0;
}

static int checkTriggerHurt (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "dmg");
	if (!*val)
		Com_Printf("  ERROR: trigger_hurt with no dmg value given - entnum: %i\n", entnum);
	return 0;
}

typedef struct entityCheck_s {
	const char *name;	/**< The entity name */
	int (*checkCallback)(entity_t* e, int entnum);		/**< @return 0 if successfully fixed it
						 * everything else in case of an error that can't be fixed automatically */
} entityCheck_t;

static const entityCheck_t checkArray[] = {
	{"worldspawn", checkWorld},
	{"light", checkLight},
	{"func_breakable", checkFuncBreakable},
	{"func_door", checkFuncDoor},
	{"func_rotating", checkFuncRotating},
	{"func_group", checkFuncGroup},
	{"misc_model", checkMiscModel},
	{"misc_particle", checkMiscParticle},
	{"misc_sound", checkMiscSound},
	{"misc_mission", checkMiscMission},
	{"misc_mission_aliens", checkMiscMission},
	{"info_player_start", checkInfoPlayerStart},
	{"info_human_start", checkStartPosition},
	{"info_alien_start", checkStartPosition},
	{"info_2x2_start", checkStartPosition},
	{"info_civilian_start", checkStartPosition},
	{"info_null", checkInfoNull},
	{"info_civilian_target", checkInfoCivilianTarget},
	{"trigger_hurt", checkTriggerHurt},

	{NULL, NULL}
};

static inline void AddBrushToList (mapbrush_t ***list, mapbrush_t *brush, int n)
{
	*list = (mapbrush_t **)realloc(*list, (n + 1) * sizeof(mapbrush_t **));
	(*list)[n] = brush;
}

static inline qboolean CheckIntersect (const mapbrush_t *b1, const mapbrush_t *b2)
{
	const boolean zNoOverlap = b1->maxs[2] < b2->mins[2] || b1->mins[2] > b2->maxs[2];
	const boolean yNoOverlap = b1->maxs[1] < b2->mins[1] || b1->mins[1] > b2->maxs[1];
	const boolean xNoOverlap = b1->maxs[0] < b2->mins[0] || b1->mins[0] > b2->maxs[0];
	return (!zNoOverlap && !yNoOverlap && !xNoOverlap);
}

static inline qboolean SidesAreAntiparallel (const side_t *side1, const side_t *side2, const float epsilon)
{
	/* calculate the cosine of the angle between side1 and side2 (dihedral angle) */
	float dot = DotProduct(side1->hessianNormal, side2->hessianNormal);
	return (dot <= -cos(epsilon));
}

/**
 * @brief calculates whether side1 faces side2. The surface unit normals
 * must be antiparallel (i.e. they face each other), and the distance
 * to the origin must be such that they occupy the same region of
 * space, to within a distance of epsilon. These are based on consideration of
 * the planes of the faces only - they could be offset by a long way.
 * @note epsilon is used as a distance in map units and an angle in radians.
 * @return true if the sides face and touch
 */
static qboolean FacingAndCoincidentTo (const side_t *side1, const side_t *side2, const float epsilon)
{
	vec3_t point;
	float distance;

	VectorMul(side2->hessianP, side2->hessianNormal, point);
	distance = abs(HessianDistance(point, side1->hessianNormal, side1->hessianP));
	return (distance < epsilon && SidesAreAntiparallel(side1, side2, epsilon));
}

/**
 * @brief Grows the brush by epsilon tolerance. this will only work for convex polyhedra.
 * @param[in] point The point to check whether it's inside the brush boundaries or not
 * @param[in] epsilon The epsilon tolerance
 * @return true if the supplied point is inside the brush
 */
static inline qboolean CheckPointInsideBrush (vec3_t point, const mapbrush_t *brush, const float epsilon)
{
	int i;

	/* if the point is on the wrong side of any face, then it is outside */
	for (i = 0; i < brush->numsides; i++) {
		const side_t *side = &brush->original_sides[i];
		if (HessianDistance(point, side->hessianNormal, side->hessianP) > epsilon)
			return qfalse;
	}
	return qtrue;
}

static void GetIntersection (const side_t *side1, const side_t *side2, const side_t *side3, vec3_t out, const float epsilon)
{
	vec3_t cross;
	vec3_t IntersectionDirSide2Side3;
	float sinAngle;
	float atobcintersection;
	float divisor;
	vec3_t term1, term2, term3;

	CrossProduct(side2->hessianNormal, side3->hessianNormal, IntersectionDirSide2Side3);
	/* sin of angle between side 2 and 3 (approx equal to angle: small angle approx) */
	sinAngle = 1.0; /*FIXME: use this one: IntersectionDirSide2Side3.magnitude();*/
	/* planes parallel, cannot intersect */
	if (sinAngle < epsilon)
		return;
	/* make it into a unit vector */
	VectorDiv(IntersectionDirSide2Side3, sinAngle, IntersectionDirSide2Side3);
	/* calculate cos of angle to normal of side 1. */
	atobcintersection = DotProduct(side1->hessianNormal, IntersectionDirSide2Side3);
	/* this is equal to sin of angle to side 1. if bc intersection line is parallel to side 1, then
	 * it can not intersect. */
	if (atobcintersection < 0 /* FIXME: Use this: Epsilon.angle */)
		return;

	/* see http://geometryalgorithms.com/Archive/algorithm_0104/algorithm_0104B.htm */
	CrossProduct(side2->hessianNormal, side3->hessianNormal, cross);
	VectorMul(-side1->hessianP, cross, term1);
	CrossProduct(side3->hessianNormal, side1->hessianNormal, cross);
	VectorMul(-side2->hessianP, cross, term2);
	CrossProduct(side1->hessianNormal, side2->hessianNormal, cross);
	VectorMul(-side3->hessianP, cross, term3);

	CrossProduct(side2->hessianNormal, side3->hessianNormal, cross);
	divisor = DotProduct(side1->hessianNormal, cross);

	VectorAdd(term1, term2, out);
	VectorAdd(out, term3, out);
	VectorDiv(out, divisor, out);
}

static int CalculateVerticesForBrush (const mapbrush_t *brush, vec3_t *list, const float epsilon)
{
	int i, j, k, n;

	n = 0;
	for (i = 1; i < brush->numsides; i++)
		for (j = 0; j < i; j++)
			for (k = j + 1; k < brush->numsides; k++) {
				const side_t *sideI = &brush->original_sides[i];
				const side_t *sideJ = &brush->original_sides[j];
				const side_t *sideK = &brush->original_sides[k];
				vec3_t candidate = {0, 0, 0};

				GetIntersection(sideI, sideJ, sideK, candidate, epsilon);
				/* if 3 faces do not intersect at point
				 * and in case 3 planes of faces intersect away from brush */
				if (VectorNotEmpty(candidate) && CheckPointInsideBrush(candidate, brush, epsilon)) {
					VectorCopy(candidate, list[n]);
					n++;
				}
			}

	return n;
}

static qboolean BrushSidesAreInside (const mapbrush_t *brush1, const mapbrush_t *brush2)
{
	int i;
	const float epsilon = 0.001;
	vec3_t vectorList[4096];
	const int n = CalculateVerticesForBrush(brush2, vectorList, epsilon);

	for (i = 0; i < brush2->numsides; i++) {
		const side_t *side = &brush2->original_sides[i];
		int j;

		for (j = 0; j < n; j++) {
			vec3_t point;
			if (abs(HessianDistance(vectorList[j], side->hessianNormal, side->hessianP)) < epsilon) {
				if (!CheckPointInsideBrush(point, brush1, epsilon))
					return qfalse;
			}
		}
	}
	return qtrue;
}

/**
 * @brief Generate a list of brushes that intersects with the given brush
 * @return A list of brushes
 * @param[in] entity The entity to check the intersecting brushes for
 */
static void CheckInteractionList (const entity_t *entity)
{
	int i, j, k, l, n;
	mapbrush_t **list;

	list = NULL;
	n = 0;

	for (i = 0; i < entity->numbrushes; i++) {
		const int brushNum = entity->firstbrush + i;
		mapbrush_t *brush = &mapbrushes[brushNum];

		/* skip translucent brushes */
		if (brush->contentFlags & CONTENTS_TRANSLUCENT)
			continue;

		for (j = 0; j < brush->numsides; j++) {
			const side_t *side = &brush->original_sides[j];
			/* skip brushes that have some special faces - note that these face
			 * flags should be the same for every face but one never knows */
			if (side->contentFlags & (CONTENTS_ORIGIN | MASK_CLIP))
				break;
		}
		/* now we can go on with the intersection calculation */
		if (j == brush->numsides) {
			for (j = 0; j < brush->numsides; j++) {
				side_t *side = &brush->original_sides[j];
				const plane_t *p = &mapplanes[side->planenum];
				vec3_t p1, p2, p3;

				VectorCopy(p->planeVector[0], p1);
				VectorCopy(p->planeVector[1], p2);
				VectorCopy(p->planeVector[2], p3);
				/* we only have to calculate these values for potential sides */
				side->hessianP = HessianNormalPlane(p1, p2, p3, side->hessianNormal);
			}

			AddBrushToList(&list, brush, n);
			n++;
		}
	}

	Sys_FPrintf(SYS_VRB, "Added %i brushes to interact with entity: %d\n", n, (int) (entity - entities));

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			const int levelFlagsI = GetLevelFlagsFromBrush(list[i]);
			const int levelFlagsJ = GetLevelFlagsFromBrush(list[j]);

			if (i != j && levelFlagsI == levelFlagsJ && CheckIntersect(list[i], list[j])) {
				for (k = 0; k < list[i]->numsides; k++) {
					side_t *sideI = &list[i]->original_sides[k];
					for (l = 0; l < list[j]->numsides; l++) {
						side_t *sideJ = &list[j]->original_sides[l];
						if (FacingAndCoincidentTo(sideI, sideJ, 0.001)) {
							if (BrushSidesAreInside(list[j], list[i])) {
								if (!(sideI->surfaceFlags & SURF_NODRAW)) {
									brush_texture_t *tex = &side_brushtextures[sideI - brushsides];
									Com_Printf("Brush %i (entity %i): set nodraw texture\n", list[i]->brushnum, list[i]->entitynum);
									Q_strncpyz(tex->name, "tex_common/nodraw", sizeof(tex->name));
									sideI->surfaceFlags |= SURF_NODRAW;
									tex->surfaceFlags |= SURF_NODRAW;
								}
							}
						}
					}
				}
			}
		}
	}

	if (n)
		free(list);
}

/**
 * @brief Perform an entity check
 */
void CheckEntities (void)
{
	int i;

	/* 0 is the world - start at 1 */
	for (i = 0; i < num_entities; i++) {
		entity_t *e = &entities[i];
		const char *name = ValueForKey(e, "classname");
		const entityCheck_t *v;

		for (v = checkArray; v->name; v++)
			if (!strncmp(name, v->name, strlen(v->name))) {
				if (v->checkCallback(e, i) != 0) {
					e->skip = qtrue;
				}
				break;
			}
		if (!v->name) {
			Com_Printf("No check for '%s' implemented\n", name);
		}

		/* check only these entities for interaction - all the others may be
		 * rotated, removed or moved */
		if (!strcmp(name, "func_group") || !strcmp(name, "worldspawn")) {
			CheckInteractionList(e);
		}
	}
}


/**
 * @returns false if the brush has a mirrored set of planes,
 * meaning it encloses no volume.
 * also checks for planes without any normal
 */
static qboolean Check_DuplicateBrushPlanes (mapbrush_t *b)
{
	int i, j;
	const side_t *sides = b->original_sides;

	for (i = 1; i < b->numsides; i++) {
		/* check for a degenerate plane */
		if (sides[i].planenum == -1) {
			Com_Printf("  Brush %i (entity %i): degenerated plane\n", b->brushnum, b->entitynum);
			continue;
		}

		/* check for duplication and mirroring */
		for (j = 0; j < i; j++) {
			if (sides[i].planenum == sides[j].planenum) {
				/* remove the second duplicate */
				Com_Printf("  Brush %i (entity %i): mirrored or duplicated\n", b->brushnum, b->entitynum);
				break;
			}

			if (sides[i].planenum == (sides[j].planenum ^ 1)) {
				Com_Printf("  Brush %i (entity %i): mirror plane - brush is invalid\n", b->brushnum, b->entitynum);
				return qfalse;
			}
		}
	}
	return qtrue;
}

/**
 * @brief sets all levelflags, if none are set.
 */
void CheckLevelFlags (void)
{
	int i, j;
	qboolean allNodraw, setFlags;
	int allLevelFlagsForBrush;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];

		/* test if all faces are nodraw */
		allNodraw=qtrue;
		for (j = 0; j < brush->numsides; j++) {
			side_t *side = &brush->original_sides[j];
			assert(side);
			if(!(side->surfaceFlags & SURF_NODRAW)){
				allNodraw = qfalse;
				break;
			}
		}

		/* proceed if some or all faces are not nodraw */
		if(!allNodraw){
			allLevelFlagsForBrush = 0;
			setFlags = qfalse;
			/* test if some faces do not have levelflags and remember
			 * all levelflags which are set. */
			for (j = 0; j < brush->numsides; j++) {
				side_t *side = &brush->original_sides[j];

				allLevelFlagsForBrush |= side->contentFlags & CONTENTS_LEVEL_ALL;

				if (!(side->contentFlags & (CONTENTS_ORIGIN | MASK_CLIP))) {
					/* check level 1 - level 8 */
					if (!(side->contentFlags & CONTENTS_LEVEL_ALL)) {
						setFlags = qtrue;
					}
				}
			}

			/* set the same flags for each face */
			if (setFlags) {
				Com_Printf("* Brush %i (entity %i): at least one face has no levelflags, setting %i on all faces\n", brush->brushnum, brush->entitynum, allLevelFlagsForBrush ? allLevelFlagsForBrush : CONTENTS_LEVEL_ALL);
				for (j = 0; j < brush->numsides; j++) {
					side_t *side = &brush->original_sides[j];
					if (allLevelFlagsForBrush){
						side->contentFlags |= allLevelFlagsForBrush;
					} else {
						side->contentFlags |= CONTENTS_LEVEL_ALL;
					}
				}
			}
		}
	}
}

/**
 * @brief check that sides have textures and that where content/surface flags are set the texture
 * is correct.
 */
void CheckTextures (void)
{
	int i, j;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];

		for (j = 0; j < brush->numsides; j++) {
			side_t *side = &brush->original_sides[j];
			const ptrdiff_t index = side - brushsides;
			brush_texture_t *tex = &side_brushtextures[index];

			assert(side);
			assert(tex);

			if (tex->name[0] == '\0') {
				Com_Printf("  Brush %i (entity %i): no texture assigned\n", brush->brushnum, brush->entitynum);
			}

			if (!Q_strcmp(tex->name, "tex_common/error")) {
				Com_Printf("  Brush %i (entity %i): error texture assigned - check this brush\n", brush->brushnum, brush->entitynum);
			}

			if (!Q_strcmp(tex->name, "NULL")) {
				Com_Printf("* Brush %i (entity %i): replaced NULL with nodraw texture\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/nodraw", sizeof(tex->name));
				side->surfaceFlags |= SURF_NODRAW;
				tex->surfaceFlags |= SURF_NODRAW;
			}
			if (side->surfaceFlags & SURF_NODRAW && Q_strcmp(tex->name, "tex_common/nodraw")) {
				Com_Printf("* Brush %i (entity %i): set nodraw texture for SURF_NODRAW\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/nodraw", sizeof(tex->name));
			}
			if (side->surfaceFlags & SURF_HINT && Q_strcmp(tex->name, "tex_common/hint")) {
				Com_Printf("* Brush %i (entity %i): set hint texture for SURF_HINT\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/hint", sizeof(tex->name));
			}

			if (side->contentFlags & CONTENTS_ACTORCLIP && side->contentFlags & CONTENTS_STEPON) {
				if (!Q_strcmp(tex->name, "tex_common/actorclip")) {
					Com_Printf("* Brush %i (entity %i): mixed CONTENTS_STEPON and CONTENTS_ACTORCLIP - removed CONTENTS_STEPON\n", brush->brushnum, brush->entitynum);
					side->contentFlags &= ~CONTENTS_STEPON;
				} else {
					Com_Printf("* Brush %i (entity %i): mixed CONTENTS_STEPON and CONTENTS_ACTORCLIP - removed CONTENTS_ACTORCLIP\n", brush->brushnum, brush->entitynum);
					side->contentFlags &= ~CONTENTS_ACTORCLIP;
				}
			}

			if (side->contentFlags & CONTENTS_WEAPONCLIP && Q_strcmp(tex->name, "tex_common/weaponclip")) {
				Com_Printf("* Brush %i (entity %i): set weaponclip texture for CONTENTS_WEAPONCLIP\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/weaponclip", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_ACTORCLIP && Q_strcmp(tex->name, "tex_common/actorclip")) {
				Com_Printf("* Brush %i (entity %i): set actorclip texture for CONTENTS_ACTORCLIP\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/actorclip", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_STEPON && Q_strcmp(tex->name, "tex_common/stepon")) {
				Com_Printf("* Brush %i (entity %i): set stepon texture for CONTENTS_STEPON\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/stepon", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_ORIGIN && Q_strcmp(tex->name, "tex_common/origin")) {
				Com_Printf("* Brush %i (entity %i): set origin texture for CONTENTS_ORIGIN\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/origin", sizeof(tex->name));
			}

		}
	}
}

void CheckBrushes (void)
{
	int i, j;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];

		/* Disabled unused variable to prevent compiler warning. */
		#if 0
		const int contentFlags = (brush->original_sides[0].contentFlags & CONTENTS_LEVEL_ALL)
			? brush->original_sides[0].contentFlags
			: (brush->original_sides[0].contentFlags | CONTENTS_LEVEL_ALL);
		#endif

		Check_DuplicateBrushPlanes(brush);

		for (j = 0; j < brush->numsides; j++) {
			side_t *side = &brush->original_sides[j];
			const ptrdiff_t index = side - brushsides;
			brush_texture_t *tex = &side_brushtextures[index];

			assert(side);
			assert(tex);

#if 1
			/* @todo remove this once every map is run with ./ufo2map -fix brushes <map> */
			/* the old footstep value */
			if (side->contentFlags & 0x00040000) {
				side->contentFlags &= ~0x00040000;
				Com_Printf("  Brush %i (entity %i): converted old footstep content to new footstep surface value\n", brush->brushnum, brush->entitynum);
				side->surfaceFlags |= SURF_FOOTSTEP;
				tex->surfaceFlags |= SURF_FOOTSTEP;
			}
			/* the old fireaffected value */
			if (side->contentFlags & 0x0008) {
				side->contentFlags &= ~0x0008;
				Com_Printf("  Brush %i (entity %i): converted old fireaffected content to new fireaffected surface value\n", brush->brushnum, brush->entitynum);
				side->surfaceFlags |= SURF_BURN;
				tex->surfaceFlags |= SURF_BURN;
			}
#endif

			if (config.performMapCheck && brush->contentFlags != side->contentFlags) {
				Com_Printf("  Brush %i (entity %i): mixed face contents (f: %i, %i)\n", brush->brushnum, brush->entitynum, brush->contentFlags, side->contentFlags);
			}

			if (side->contentFlags & CONTENTS_ORIGIN && brush->entitynum == 0) {
				Com_Printf("* Brush %i (entity %i): origin brush inside worldspawn - removed CONTENTS_ORIGIN\n", brush->brushnum, brush->entitynum);
				side->contentFlags &= ~CONTENTS_ORIGIN;
			}
		}
	}
}


