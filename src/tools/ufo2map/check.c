/**
 * @file check.c
 * @brief Some checks during compile, warning on -check and changes .map on -fix
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
#include "ufo2map.h"

#define MANDATORY_KEY 1
#define NON_MANDATORY_KEY 0

/** how close faces have to be in order for one to be hidden and set to SURF_NODRAW. Also
 *  the margin for abutting brushes to be considered not intersecting */
#define CH_DIST_EPSILON 0.001f

/** if the cosine of an angle is greater than this, then the angle is negligibly different from zero */
#define COS_EPSILON 0.9999f

/** if the sine of an angle is less than this, then the angle is negligibly different from zero */
#define SIN_EPSILON 0.0001f

/**
 * @brief wether the surface of a brush is included when testing if a point is in a brush
 * determines how epsilon is applied.
 * @sa Check_IsPointInsideBrush
 */
typedef enum {
	PIB_EXCL_SURF, 				/**< surface is excluded */
	PIB_INCL_SURF_EXCL_EDGE,	/**< surface is included, but edges of brush are excluded */
	PIB_INCL_SURF,				/**< surface is included */
	PIB_ON_SURFACE_ONLY			/**< point on the surface, and the inside of the brush is excluded */
} pointInBrush_t;

static void Check_Printf(const verbosityLevel_t msgVerbLevel, const char *format, ...) __attribute__((format(printf, 2, 3)));

/**
 * @brief decides wether to proceed with output based on ufo2map's mode: check/fix/compile
 * @sa Com_Printf, Verb_Printf
 */
static void Check_Printf (const verbosityLevel_t msgVerbLevel, const char *format, ...)
{
	static int skippingCheckLine = 0;
	static qboolean firstSuccessfulPrint = qtrue;

	if (AbortPrint(msgVerbLevel))
		return;

	/* some checking/fix functions are called when ufo2map is compiling
	 * then the check/fix functions should be quiet */
	if (!(config.performMapCheck || config.fixMap))
		return;

	/* output prefixed with "  " is only a warning, should not be
	 * be displayed in fix mode. may be sent here in several function calls.
	 * skip everything from start of line "  " to \n */
	if (config.fixMap) {
		const qboolean startOfWarning = (format[0] == ' ' && format[1] == ' ');
		const qboolean containsNewline = strchr(format, '\n') != NULL;

		/* skip output sent in single call */
		if (!skippingCheckLine && startOfWarning && containsNewline)
			return;

		/* enter multi-call skip mode */
		if (!skippingCheckLine && startOfWarning) {
			skippingCheckLine = 1;
			return;
		}

		/* leave multi-call skip mode */
		if (skippingCheckLine && containsNewline) {
			skippingCheckLine = 0;
			return;
		}

		/* middle of multi-call skip mode */
		if (skippingCheckLine)
			return;
	}

	if (firstSuccessfulPrint && config.verbosity == VERB_MAPNAME) {
		PrintName();
		firstSuccessfulPrint = qfalse;
	}

	{
		char out_buffer[4096];
		va_list argptr;

		va_start(argptr, format);
		Q_vsnprintf(out_buffer, sizeof(out_buffer), format, argptr);
		va_end(argptr);

		printf("%s", out_buffer);
	}
}

/**
 * @param[in] mandatory if this key is missing the entity will be deleted, else just a warning
 */
static int checkEntityKey (entity_t *e, const int entnum, const char* key, int mandatory)
{
	const char *val = ValueForKey(e, key);
	const char *name = ValueForKey(e, "classname");
	if (!*val) {
		if (mandatory == MANDATORY_KEY) {
			Check_Printf(VERB_CHECK, "* Entity %i: %s with no %s given - will be deleted\n", entnum, name, key);
			return 1;
		} else {
			Check_Printf(VERB_CHECK, "  Entity %i: %s with no %s given\n", entnum, name, key);
			return 0;
		}
	}

	return 0;
}

static void checkEntityLevelFlags (entity_t *e, const int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	const char *name = ValueForKey(e, "classname");
	if (!*val) {
		char buf[16];
		Check_Printf(VERB_CHECK, "* Entity %i: %s with no levelflags given - setting all\n", entnum, name);
		snprintf(buf, sizeof(buf) - 1, "%i", (CONTENTS_LEVEL_ALL >> 8));
		SetKeyValue(e, "spawnflags", buf);
	}
}

static int checkEntityZeroBrushes (entity_t *e, int entnum)
{
	const char *name = ValueForKey(e, "classname");
	if (!e->numbrushes) {
		Check_Printf(VERB_CHECK, "* Entity %i: %s with no brushes given - will be deleted\n", entnum, name);
		return 1;
	}
	return 0;
}

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
	checkEntityLevelFlags(e, entnum);

	if (checkEntityZeroBrushes(e, entnum))
		return 1;

	return 0;
}

static int checkFuncDoor (entity_t *e, int entnum)
{
	checkEntityLevelFlags(e, entnum);

	if (checkEntityZeroBrushes(e, entnum))
		return 1;

	return 0;
}

static int checkFuncBreakable (entity_t *e, int entnum)
{
	checkEntityLevelFlags(e, entnum);

	if (checkEntityZeroBrushes(e, entnum)) {
		return 1;
	} else if (e->numbrushes > 1) {
		Check_Printf(VERB_CHECK, "  Entity %i: func_breakable with more than one brush given (might break pathfinding)\n", entnum);
	}

	return 0;
}

static int checkMiscItem (entity_t *e, int entnum)
{
	if (checkEntityKey(e, entnum, "item", MANDATORY_KEY))
		return 1;

	return 0;
}

static int checkMiscModel (entity_t *e, int entnum)
{
	checkEntityLevelFlags(e, entnum);

	if (checkEntityKey(e, entnum, "model", MANDATORY_KEY))
		return 1;

	return 0;
}

static int checkMiscParticle (entity_t *e, int entnum)
{
	checkEntityLevelFlags(e, entnum);

	if (checkEntityKey(e, entnum, "particle", MANDATORY_KEY))
		return 1;

	return 0;
}

static int checkMiscMission (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "health");
	if (!*val)
		val = ValueForKey(e, "time");
	if (!*val) {
		val = ValueForKey(e, "target");
		if (*val && !FindTargetEntity(val))
			Check_Printf(VERB_CHECK, "  ERROR: misc_mission could not find specified target: '%s' - entnum: %i\n", val, entnum);
	}
	if (!*val)
		Check_Printf(VERB_CHECK, "  ERROR: misc_mission with no objectives given - entnum: %i\n", entnum);
	return 0;
}

static int checkFuncGroup (entity_t *e, int entnum)
{
	if (checkEntityZeroBrushes(e, entnum))
		return 1;

	return 0;
}

static int checkStartPosition (entity_t *e, int entnum)
{
	int align = 16;
	const char *val = ValueForKey(e, "classname");

	if (!strcmp(val, "info_2x2_start"))
		align = 32;

	if (((int)e->origin[0] - align) % UNIT_SIZE || ((int)e->origin[1] - align) % UNIT_SIZE) {
		Check_Printf(VERB_CHECK, "* ERROR: misaligned starting position - entnum: %i (%i: %i). The %s will be deleted\n", entnum, (int)e->origin[0], (int)e->origin[1], val);
		return 1; /** @todo auto-align entity and check for intersection with brush */
	}
	return 0;
}

static int checkInfoPlayerStart (entity_t *e, int entnum)
{
	if (checkEntityKey(e, entnum, "team", MANDATORY_KEY))
		return 1;

	return checkStartPosition(e, entnum);
}

static int checkInfoNull (entity_t *e, int entnum)
{
	if (checkEntityKey(e, entnum, "targetname", MANDATORY_KEY))
		return 1;

	return 0;
}

static int checkInfoCivilianTarget (entity_t *e, int entnum)
{
	checkEntityKey(e, entnum, "count", NON_MANDATORY_KEY);
	return 0;
}

static int checkMiscSound (entity_t *e, int entnum)
{
	if (checkEntityKey(e, entnum, "noise", MANDATORY_KEY))
		return 1;
	return 0;
}

static int checkTriggerHurt (entity_t *e, int entnum)
{
	checkEntityKey(e, entnum, "dmg", NON_MANDATORY_KEY);
	return 0;
}

static int checkTriggerTouch (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "target");
	if (!*val)
		Check_Printf(VERB_CHECK, "  ERROR: trigger_touch with no target given - entnum: %i\n", entnum);
	else if (!FindTargetEntity(val))
		Check_Printf(VERB_CHECK, "  ERROR: trigger_touch could not find specified target: '%s' - entnum: %i\n", val, entnum);
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
	{"misc_item", checkMiscItem},
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
	{"trigger_touch", checkTriggerTouch},

	{NULL, NULL}
};

/**
 * @brief distance from a point to a plane.
 * @note the sign of the result depends on which side of the plane the point is
 * @return a negative distance if the point is on the inside of the plane
 */
static inline float Check_PointPlaneDistance (const vec3_t point, const plane_t *plane)
{
	/* normal should have a magnitude of one */
	assert(abs(VectorLengthSqr(plane->normal) - 1.0f) < CH_DIST_EPSILON);

	return DotProduct(point, plane->normal) - plane->dist;
}

/**
 * @brief calculates whether side1 faces side2 and touches.
 * @details The surface unit normals
 * must be antiparallel (i.e. they face each other), and the distance
 * to the origin must be such that they occupy the same region of
 * space, to within a distance of epsilon. These are based on consideration of
 * the planes of the faces only - they could be offset by a long way.
 * @note carefully tested around r18537 by blondandy
 * @return true if the sides face and touch
 */
static qboolean FacingAndCoincidentTo (const side_t *side1, const side_t *side2)
{
	plane_t *plane1 = &mapplanes[side1->planenum];
	plane_t *plane2 = &mapplanes[side2->planenum];
	float distance, dihedralCos;

	dihedralCos = DotProduct(plane1->normal, plane2->normal);
	if (dihedralCos >= -COS_EPSILON)
		return qfalse; /* not facing each other */

	/* calculate the distance of point from plane2. as we have established that the
	 * plane's normals are antiparallel, and plane1->planeVector[0] is a point on plane1
	 * (that was supplied in the map file), this is the distance
	 * between the planes */
	distance = Check_PointPlaneDistance(plane1->planeVector[0], plane2);

	return abs(distance) < CH_DIST_EPSILON;
}

/**
 * @brief tests if a point is in a map brush.
 * @param[in] point The point to check whether it's inside the brush boundaries or not
 * @param[in] mode determines how epsilons are applied
 * @return qtrue if the supplied point is inside the brush
 */
static inline qboolean Check_IsPointInsideBrush (const vec3_t point, const mapbrush_t *brush, const pointInBrush_t mode)
{
	int i;
	float dist; /* distance to one of the planes of the sides, negative implies the point is inside this plane */
	int numPlanes = 0; /* how many of the sides the point is on. on 2 sides, means on an edge. on 3 a vertex */
	float epsilon = CH_DIST_EPSILON;

	/* PIB_INCL_SURF is the default */
	epsilon *= mode == PIB_EXCL_SURF ? -1.0f : 1.0f;/* apply epsilon the other way if the surface is excluded */

	for (i = 0; i < brush->numsides; i++) {
		const plane_t *plane = &mapplanes[brush->original_sides[i].planenum];

			/* if the point is on the wrong side of any face, then it is outside */
		dist = Check_PointPlaneDistance(point, plane);
		if (dist > epsilon)
			return qfalse;

		numPlanes += abs(dist) < CH_DIST_EPSILON ? 1 : 0;
	}

	if (mode == PIB_ON_SURFACE_ONLY && numPlanes == 0)
		return qfalse; /* must be on at least one surface */

	if (mode == PIB_INCL_SURF_EXCL_EDGE && numPlanes > 1)
		return qfalse; /* must not be on more than one side, that would be an edge */

	/* inside all planes, therefore inside the brush */
	return qtrue;
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
					e->skip = qtrue; /* skip: the entity will not be saved back on -fix */
				}
				break;
			}
		if (!v->name) {
			Check_Printf(VERB_CHECK, "No check for '%s' implemented\n", name);
		}
	}
}


/**
 * @return qtrue for brushes that do not move, are not breakable, are not seethrough, etc
 */
static qboolean Check_IsOptimisable (const mapbrush_t *b) {
	const entity_t *e = &entities[b->entitynum];
	const char *name = ValueForKey(e, "classname");
	int i, numNodraws = 0;

	if (strcmp(name, "func_group") && strcmp(name, "worldspawn"))
		return qfalse;/* other entities, eg func_breakable are no use */

	/* content flags should be the same on all faces, but we shall be suspicious */
	for (i = 0; i < b->numsides; i++) {
		const side_t *side = &b->original_sides[i];
		if (side->contentFlags & (CONTENTS_ORIGIN | MASK_CLIP | CONTENTS_TRANSLUCENT))
			return qfalse;
		numNodraws += side->surfaceFlags & SURF_NODRAW ? 1 : 0;
	}

	/* all nodraw brushes are special too */
	return numNodraws == b->numsides ? qfalse : qtrue;
}

/**
 * @return qtrue if the bounding boxes intersect or are within CH_DIST_EPSILON of intersecting
 */
static qboolean Check_BoundingBoxIntersects (const mapbrush_t *a, const mapbrush_t *b)
{
	int i;

	for (i = 0; i < 3; i++)
		if (a->mins[i] - CH_DIST_EPSILON >= b->maxs[i] || a->maxs[i] <= b->mins[i] - CH_DIST_EPSILON)
			return qfalse;

	return qtrue;
}

/**
 * @brief add a list of near brushes to each mapbrush. near meaning that the bounding boxes
 * are intersecting or within CH_DIST_EPSILON of touching.
 * @warning includes changeable brushes: mostly non-optimisable brushes will need to be excluded.
 * @sa Check_IsOptimisable
 */
static void Check_NearList (void)
{
	/* this function may be called more than once, but we only want this done once */
	static qboolean done = qfalse;
	mapbrush_t *bbuf[MAX_MAP_BRUSHES];/*< store pointers to brushes here and then malloc them when we know how many */
	int i, j, numNear;

	if (done)
		return;

	/* make a list for iBrush*/
	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *iBrush = &mapbrushes[i];

		/* test all brushes for nearness to iBrush */
		for (j = 0, numNear = 0 ; j < nummapbrushes; j++) {
			mapbrush_t *jBrush = &mapbrushes[j];

			if (i == j) /* do not list a brush as being near itself - not useful!*/
				continue;

			if (!Check_BoundingBoxIntersects(iBrush, jBrush))
				continue;

			/* near, therefore add to temp list for iBrush */
			assert(numNear < nummapbrushes);
			bbuf[numNear++] = jBrush;
		}

		/* now we know how many, we can malloc. then copy the pointers */
		iBrush->nearBrushes = (mapbrush_t **)malloc(numNear * sizeof(mapbrush_t *));

		if (!iBrush->nearBrushes)
			Sys_Error("Check_Nearlist: out of memory");

		iBrush->numNear = numNear;
		for (j = 0; j < numNear; j++) {
			iBrush->nearBrushes[j] = bbuf[j];
		}
	}

	done = qtrue;
}

/**
 * @brief 	calculate where an edge (defined by the vertices) intersects a plane.
 *			http://local.wasp.uwa.edu.au/~pbourke/geometry/planeline/
 * @param[out] the position of the intersection, if the edge is not too close to parallel.
 * @return 	zero if the edge is within an epsilon angle of parallel to the plane, or the edge is near zero length.
 * @note	an epsilon is used to exclude the actual vertices from passing the test.
 */
static int Check_EdgePlaneIntersection (const vec3_t vert1, const vec3_t vert2, const plane_t *plane, vec3_t intersection)
{
	vec3_t direction; /*< a vector in the direction of the line */
	vec3_t lineToPlane; /*< a line from vert1 on the line to a point on the plane */
	float sin; /*< sine of angle to plane, cosine of angle to normal */
	float param; /*< param in line equation  line = vert1 + param * (vert2 - vert1) */
	float length; /*< length of the edge */

	VectorSubtract(vert2, vert1, direction);/*< direction points from vert1 to vert2 */
	length = VectorLength(direction);
	if (length < DIST_EPSILON)
		return qfalse;
	sin = DotProduct(direction, plane->normal) / length;
	if (abs(sin) < SIN_EPSILON)
		return qfalse;
	VectorSubtract(plane->planeVector[0], vert1, lineToPlane);
	param = DotProduct(plane->normal, lineToPlane) / DotProduct(plane->normal, direction);
	VectorMul(param, direction, direction);/*< now direction points from vert1 to intersection */
	VectorAdd(vert1, direction, intersection);
	param = param * length;/*< param is now the distance along the edge from vert1 */
	return (param > CH_DIST_EPSILON) && (param < (length - CH_DIST_EPSILON));
}

/**
 * @brief tests the lines joining the vertices in the winding
 * @return qtrue if the any lines intersect the brush
 */
static qboolean Check_WindingIntersects (const winding_t *winding, const mapbrush_t *brush)
{
	vec3_t intersection;
	int vi, vj, bi;
	for (bi = 0; bi < brush->numsides; bi++) {
		for (vi = 0; vi < winding->numpoints; vi++) {
			vj = vi + 1;
			vj = vj == winding->numpoints ? 0 : vj;
			if (Check_EdgePlaneIntersection(winding->p[vi], winding->p[vj], &mapplanes[brush->original_sides[bi].planenum], intersection))
				if (Check_IsPointInsideBrush(intersection, brush, PIB_INCL_SURF_EXCL_EDGE)) {
					#if 0
					Print3Vector(intersection);
					#else
					return qtrue;
					#endif
			}
		}
	}
	return qfalse;
}

/**
 * @brief reports intersection between optimisable map brushes
 */
void Check_BrushIntersection (void)
{
	int i, j, is;

	/* initialise mapbrush_t.nearBrushes */
	Check_NearList();

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *iBrush = &mapbrushes[i];

		if(!Check_IsOptimisable(iBrush))
			continue;

		for (j = 0; j < iBrush->numNear; j++) {
			mapbrush_t *jBrush = iBrush->nearBrushes[j];

			if(!Check_IsOptimisable(jBrush))
				continue;

			/* check each side of i for intersection with brush j */
			for (is = 0; is < iBrush->numsides; is++) {
				winding_t *winding = (iBrush->original_sides[is].winding);
				if (Check_WindingIntersects(winding, jBrush)) {
					Check_Printf(VERB_CHECK, "  Brush %i (entity %i): intersects with brush %i (entity %i)\n", iBrush->brushnum, iBrush->entitynum, jBrush->brushnum, jBrush->entitynum);
					break;
				}
			}
		}
	}
}

/**
 * @brief tests the vertices in the winding of side s.
 * @param[in] mode determines how epsilon is applied
 * @return qtrue if they are all in or on (within epsilon) brush b
 * @sa Check_IsPointInsideBrush
 */
static qboolean Check_SideIsInBrush (const side_t *side, const mapbrush_t *brush, pointInBrush_t mode)
{
	int i;
	const winding_t *w = side->winding;

	assert(w->numpoints > 0);

	for (i = 0; i < w->numpoints ; i++)
		if (!Check_IsPointInsideBrush(w->p[i], brush, mode))
			return qfalse;

	return qtrue;
}

/**
 * @brief find duplicated brushes and brushes contained inside brushes
 */
void Check_ContainedBrushes (void)
{
	int i, j, js;

	/* initialise mapbrush_t.nearBrushes */
	Check_NearList();

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *iBrush = &mapbrushes[i];

		/* do not check for brushes inside special (clip etc) brushes */
		if (!Check_IsOptimisable(iBrush))
			continue;

		for (j = 0; j < iBrush->numNear; j++) {
			mapbrush_t *jBrush = iBrush->nearBrushes[j];
			int numSidesInside = 0;

			for (js = 0; js < jBrush->numsides; js++) {
				side_t *jSide = &jBrush->original_sides[js];

				if (Check_SideIsInBrush(jSide, iBrush, PIB_INCL_SURF))
					numSidesInside++;
			}

			if (numSidesInside == jBrush->numsides) {
				Check_Printf(VERB_CHECK, "  Brush %i (entity %i): is inside brush %i (entity %i)%s\n",
							jBrush->brushnum, jBrush->entitynum,
							iBrush->brushnum, iBrush->entitynum,
							Check_IsOptimisable(iBrush) ? "" : " - changeable, clip, translucent or origin");

			}
		}
	}
}

/**
 * @return nonzero if for any level selection the coveree will only be hidden when the coverer is too.
 * so the coveree may safely be set to nodraw, as far as levelflags are concerned.
 */
static int Check_LevelForNodraws (const side_t *coverer, const side_t *coveree)
{
	return !(CONTENTS_LEVEL_ALL & ~coverer->contentFlags & coveree->contentFlags);
}

/**
 * @brief Check for SURF_NODRAW which might be exposed, and check for
 * faces which can safely be set to SURF_NODRAW because they are pressed against
 * the faces of other brushes.
 * @todo test for sides hidden by composite faces
 * @todo warn about faces which are nodraw, but might be visible
 */
void CheckNodraws (void)
{
	int i, j, is, js;

	/* initialise mapbrush_t.nearBrushes */
	Check_NearList();

	/* check each brush, i, for hidden sides */
	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *iBrush = &mapbrushes[i];
		int numSet = 0;

		/* skip moving brushes, clips etc */
		if (!Check_IsOptimisable(iBrush))
			continue;

		/* check each brush, j, for having a side that hides one of i's faces */
		for (j = 0; j < iBrush->numNear; j++) {
			mapbrush_t *jBrush = iBrush->nearBrushes[j];

			/* skip moving brushes, clips etc */
			if (!Check_IsOptimisable(jBrush))
				continue;

			/* check each side of i for being hidden */
			for (is = 0; is < iBrush->numsides; is++) {
			side_t *iSide = &iBrush->original_sides[is];

				/* check each side of brush j for doing the hiding */
				for (js = 0; js < jBrush->numsides; js++) {
					side_t *jSide = &jBrush->original_sides[js];

					if (Check_LevelForNodraws(jSide, iSide) &&
						FacingAndCoincidentTo(iSide, jSide) &&
						Check_SideIsInBrush(iSide, jBrush, PIB_INCL_SURF)) {

						const ptrdiff_t index = iSide - brushsides;
						brush_texture_t *tex = &side_brushtextures[index];
						Q_strncpyz(tex->name, "tex_common/nodraw", sizeof(tex->name));
						iSide->surfaceFlags |= SURF_NODRAW;
						tex->surfaceFlags |= SURF_NODRAW;
						numSet++;

					}
				}
			}
		}
		if (numSet)
			Check_Printf(VERB_CHECK, "* Brush %i (entity %i): set nodraw on %i sides (covered by another brush).\n", iBrush->brushnum, iBrush->entitynum, numSet);
	}

}

/**
 * @returns false if the brush has a mirrored set of planes,
 * meaning it encloses no volume.
 * also checks for planes without any normal
 */
static qboolean Check_DuplicateBrushPlanes (const mapbrush_t *b)
{
	int i, j;
	const side_t *sides = b->original_sides;

	for (i = 1; i < b->numsides; i++) {
		/* check for a degenerate plane */
		if (sides[i].planenum == -1) {
			Check_Printf(VERB_CHECK, "  Brush %i (entity %i): degenerated plane\n", b->brushnum, b->entitynum);
			continue;
		}

		/* check for duplication and mirroring */
		for (j = 0; j < i; j++) {
			if (sides[i].planenum == sides[j].planenum) {
				/* remove the second duplicate */
				Check_Printf(VERB_CHECK, "  Brush %i (entity %i): mirrored or duplicated\n", b->brushnum, b->entitynum);
				break;
			}

			if (sides[i].planenum == (sides[j].planenum ^ 1)) {
				Check_Printf(VERB_CHECK, "  Brush %i (entity %i): mirror plane - brush is invalid\n", b->brushnum, b->entitynum);
				return qfalse;
			}
		}
	}
	return qtrue;
}

/**
 * @sa BrushVolume
 */
static vec_t Check_MapBrushVolume (mapbrush_t *brush)
{
	int i;
	winding_t *w;
	vec3_t corner;
	vec_t d, area, volume;
	plane_t *plane;

	if (!brush)
		return 0;

	/* grab the first valid point as the corner */
	w = NULL;
	for (i = 0; i < brush->numsides; i++) {
		w = brush->original_sides[i].winding;
		if (w)
			break;
	}
	if (!w)
		return 0;
	VectorCopy(w->p[0], corner);

	/* make tetrahedrons to all other faces */
	volume = 0;
	for (; i < brush->numsides; i++) {
		w = brush->original_sides[i].winding;
		if (!w)
			continue;
		plane = &mapplanes[brush->original_sides[i].planenum];
		d = -(DotProduct(corner, plane->normal) - plane->dist);
		area = WindingArea(w);
		volume += d * area;
	}

	return volume / 3;
}

/**
 * @brief report brushes from the map below 1 unit^3
 */
 void CheckMapMicro (void)
 {
	int i;
	float vol;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];
		vol = Check_MapBrushVolume(brush);
		if (vol < config.mapMicrovol)
			Check_Printf(VERB_CHECK, "  Brush %i (entity %i): warning, microbrush: volume %f\n", brush->brushnum, brush->entitynum, vol);

	}
 }

/**
 * @brief prints a list of the names of the set content flags or "no contentflags" if all bits are 0
 */
void DisplayContentFlags (const int flags)
{
	if (!flags) {
		Check_Printf(VERB_CHECK, " no contentflags");
		return;
	}
#define M(x) if (flags & CONTENTS_##x) Check_Printf(VERB_CHECK, " " #x)
	M(SOLID);
	M(WINDOW);
	M(WATER);
	M(LEVEL_1);
	M(LEVEL_2);
	M(LEVEL_3);
	M(LEVEL_4);
	M(LEVEL_5);
	M(LEVEL_6);
	M(LEVEL_7);
	M(LEVEL_8);
	M(ACTORCLIP);
	M(PASSABLE);
	M(ACTOR);
	M(ORIGIN);
	M(WEAPONCLIP);
	M(DEADACTOR);
	M(DETAIL);
	M(TRANSLUCENT);
	M(STEPON);
#undef x
}

/**
 * @brief calculate the bits that have to be set to fill levelflags such that they are contiguous
 */
static int Check_CalculateLevelFlagFill(int contentFlags)
{
	int firstSetLevel = 0, lastSetLevel=0;
	int scanLevel, flagFill = 0;
	for (scanLevel = CONTENTS_LEVEL_1; scanLevel <= CONTENTS_LEVEL_8; scanLevel <<= 1) {
		if (scanLevel & contentFlags) {
			if (!firstSetLevel) {
				firstSetLevel = scanLevel;
			} else {
				lastSetLevel = scanLevel;
			}
		}
	}
	for (scanLevel = firstSetLevel << 1 ; scanLevel < lastSetLevel; scanLevel <<= 1)
		flagFill |= scanLevel & ~contentFlags;
	return flagFill;
}

/**
 * @brief ensures set levelflags are in one contiguous block
 */
void CheckFillLevelFlags (void)
{
	int i, j, flagFill;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];

		/* CheckLevelFlags should be done first, so we will boldly
		 * assume that levelflags are the same on each face */
		flagFill = Check_CalculateLevelFlagFill(brush->original_sides[0].contentFlags);
		if (flagFill) {
			Check_Printf(VERB_CHECK, "* Brush %i (entity %i): making set levelflags continuous by setting", brush->brushnum, brush->entitynum);
			DisplayContentFlags(flagFill);
			Check_Printf(VERB_CHECK, "\n");
			for (j = 0; j < brush->numsides; j++)
					brush->original_sides[0].contentFlags |= flagFill;
		}
	}
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
		allNodraw = qtrue;
		for (j = 0; j < brush->numsides; j++) {
			side_t *side = &brush->original_sides[j];
			assert(side);

			if (!(side->surfaceFlags & SURF_NODRAW)) {
				allNodraw = qfalse;
				break;
			}
		}

		/* proceed if some or all faces are not nodraw */
		if (!allNodraw) {
			allLevelFlagsForBrush = 0;

			setFlags = qfalse;
			/* test if some faces do not have levelflags and remember
			 * all levelflags which are set. */
			for (j = 0; j < brush->numsides; j++) {
				side_t *side = &brush->original_sides[j];

				allLevelFlagsForBrush |= (side->contentFlags & CONTENTS_LEVEL_ALL);

				if (!(side->contentFlags & (CONTENTS_ORIGIN | MASK_CLIP))) {
					/* check level 1 - level 8 */
					if (!(side->contentFlags & CONTENTS_LEVEL_ALL)) {
						setFlags = qtrue;
						break;
					}
				}
			}

			/* set the same flags for each face */
			if (setFlags) {
				const int flagsToSet = allLevelFlagsForBrush ? allLevelFlagsForBrush : CONTENTS_LEVEL_ALL;
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): at least one face has no levelflags, setting %i on all faces\n", brush->brushnum, brush->entitynum, flagsToSet);
				for (j = 0; j < brush->numsides; j++) {
					side_t *side = &brush->original_sides[j];
					side->contentFlags |= flagsToSet;
				}
			}
		}
	}
}

/**
 * @brief Sets surface flags dependent on assigned texture
 * @sa ParseBrush
 * @sa CheckFlags
 * @note surfaceFlags are set in side_t for map compiling and in brush_texture_t
 * because this is saved back on -fix.
 */
void SetImpliedFlags (side_t *side, brush_texture_t *tex, const mapbrush_t *brush)
{
	const char *texname = tex->name;
	const int initSurf = tex->surfaceFlags;
	const int initCont = side->contentFlags;
	const char *flagsDescription = NULL;

	if (!strcmp(texname, "tex_common/actorclip")) {
		side->contentFlags |= CONTENTS_ACTORCLIP;
		flagsDescription = "CONTENTS_ACTORCLIP";
	} else if (!strcmp(texname, "tex_common/caulk")) {
		side->surfaceFlags |= SURF_NODRAW;
		tex->surfaceFlags |= SURF_NODRAW;
		flagsDescription = "SURF_NODRAW";
	} else if (!strcmp(texname, "tex_common/hint")) {
		side->surfaceFlags |= SURF_HINT;
		tex->surfaceFlags |= SURF_HINT;
		flagsDescription = "SURF_HINT";
	} else if (!strcmp(texname, "tex_common/nodraw")) {
		side->surfaceFlags |= SURF_NODRAW;
		tex->surfaceFlags |= SURF_NODRAW;
		flagsDescription = "SURF_NODRAW";
	} else if (!strcmp(texname, "tex_common/trigger")) {
		side->surfaceFlags |= SURF_NODRAW;
		tex->surfaceFlags |= SURF_NODRAW;
		flagsDescription = "SURF_NODRAW";
	} else if (!strcmp(texname, "tex_common/origin")) {
		side->contentFlags |= CONTENTS_ORIGIN;
		flagsDescription = "CONTENTS_ORIGIN";
	} else if (!strcmp(texname, "tex_common/slick")) {
		side->contentFlags |= SURF_SLICK;
		flagsDescription = "SURF_SLICK";
	} else if (!strcmp(texname, "tex_common/stepon")) {
		side->contentFlags |= CONTENTS_STEPON;
		flagsDescription = "CONTENTS_STEPON";
	} else if (!strcmp(texname, "tex_common/weaponclip")) {
		side->contentFlags |= CONTENTS_WEAPONCLIP;
		flagsDescription = "CONTENTS_WEAPONCLIP";
	}

	if (strstr(texname, "water")) {
		#if 0
		side->surfaceFlags |= SURF_WARP;
		tex->surfaceFlags |= SURF_WARP;
		#endif
		side->contentFlags |= CONTENTS_WATER;
		side->contentFlags |= CONTENTS_PASSABLE;
		flagsDescription = "CONTENTS_WATER and CONTENTS_PASSABLE";
	}

	/* If in check/fix mode and we have made a change, give output. */
	if ((side->contentFlags != initCont) || (tex->surfaceFlags != initSurf)) {
		Check_Printf(VERB_CHECK, "* Brush %i (entity %i): %s implied by %s texture has been set\n",
			brush->brushnum, brush->entitynum, flagsDescription ? flagsDescription : "-", texname);
	}

	/*one additional test, which does not directly depend on tex. */
	if ((tex->surfaceFlags & SURF_NODRAW) && (tex->surfaceFlags & SURF_PHONG)) {
		/* nodraw never has phong set */
		side->surfaceFlags &= ~SURF_PHONG;
		tex->surfaceFlags &= ~SURF_PHONG;
		Check_Printf(VERB_CHECK, "* Brush %i (entity %i): SURF_PHONG unset, as it has SURF_NODRAW set\n",
				brush->brushnum, brush->entitynum);
	}
}

/**
 * @brief sets content flags based on textures
 */
void CheckFlagsBasedOnTextures (void)
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

			/* set surface and content flags based on texture. */
			SetImpliedFlags(side, tex, brush);
		}
	}
}

/**
 * @brief check that sides have textures and that where content/surface flags are set the texture
 * is correct.
 */
void CheckTexturesBasedOnFlags (void)
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

			/* set textures based on flags */
			if (tex->name[0] == '\0') {
				Check_Printf(VERB_CHECK, "  Brush %i (entity %i): no texture assigned\n", brush->brushnum, brush->entitynum);
			}

			if (!Q_strcmp(tex->name, "tex_common/error")) {
				Check_Printf(VERB_CHECK, "  Brush %i (entity %i): error texture assigned - check this brush\n", brush->brushnum, brush->entitynum);
			}

			if (!Q_strcmp(tex->name, "NULL")) {
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): replaced NULL with nodraw texture\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/nodraw", sizeof(tex->name));
				tex->surfaceFlags |= SURF_NODRAW;
			}
			if (tex->surfaceFlags & SURF_NODRAW && Q_strcmp(tex->name, "tex_common/nodraw")) {
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): set nodraw texture for SURF_NODRAW\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/nodraw", sizeof(tex->name));
			}
			if (tex->surfaceFlags & SURF_HINT && Q_strcmp(tex->name, "tex_common/hint")) {
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): set hint texture for SURF_HINT\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/hint", sizeof(tex->name));
			}

			if (side->contentFlags & CONTENTS_ACTORCLIP && side->contentFlags & CONTENTS_STEPON) {
				if (!Q_strcmp(tex->name, "tex_common/actorclip")) {
					Check_Printf(VERB_CHECK, "* Brush %i (entity %i): mixed CONTENTS_STEPON and CONTENTS_ACTORCLIP - removed CONTENTS_STEPON\n", brush->brushnum, brush->entitynum);
					side->contentFlags &= ~CONTENTS_STEPON;
				} else {
					Check_Printf(VERB_CHECK, "* Brush %i (entity %i): mixed CONTENTS_STEPON and CONTENTS_ACTORCLIP - removed CONTENTS_ACTORCLIP\n", brush->brushnum, brush->entitynum);
					side->contentFlags &= ~CONTENTS_ACTORCLIP;
				}
			}

			if (side->contentFlags & CONTENTS_WEAPONCLIP && Q_strcmp(tex->name, "tex_common/weaponclip")) {
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): set weaponclip texture for CONTENTS_WEAPONCLIP\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/weaponclip", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_ACTORCLIP && Q_strcmp(tex->name, "tex_common/actorclip")) {
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): set actorclip texture for CONTENTS_ACTORCLIP\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/actorclip", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_STEPON && Q_strcmp(tex->name, "tex_common/stepon")) {
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): set stepon texture for CONTENTS_STEPON\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/stepon", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_ORIGIN && Q_strcmp(tex->name, "tex_common/origin")) {
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): set origin texture for CONTENTS_ORIGIN\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/origin", sizeof(tex->name));
			}
		}
	}
}

/**
 * @brief some contentlflags are set as a result of some surface flag. For example,
 * if one face is TRANS* then the brush is TRANSLUCENT. this is required by the .map parser
 * as well as th check/fix code.
 * @sa ParseBrush
 */
void CheckPropagateParserContentFlags(mapbrush_t *b)
{
	int notInformedMixedFace = 1;
	int m, contentFlagDiff;
	int transferFlags = (CONTENTS_DETAIL | CONTENTS_TRANSLUCENT);

	for (m = 0; m < b->numsides; m++) {
		contentFlagDiff = (b->original_sides[m].contentFlags ^ b->contentFlags) & transferFlags;
		if (contentFlagDiff) {
			/* only tell them once per brush */
			if (notInformedMixedFace) {
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): transferring contentflags to all faces:", b->brushnum, b->entitynum);
				DisplayContentFlags(contentFlagDiff);
				Check_Printf(VERB_CHECK, "\n");
				notInformedMixedFace = 0;
			}
			b->original_sides[m].contentFlags |= b->contentFlags ;
		}
	}
}

/**
 * @brief contentflags should be the same on each face of a brush. print warnings
 * if they are not. remove contentflags that are set on less than half of the faces.
 * some content flags are transferred to all faces on parsing, ParseBrush().
 * @todo at the moment only actorclip is removed if only set on less than half of the
 * faces. there may be other contentflags that would benefit from this treatment
 * @sa ParseBrush
 */
void CheckMixedFaceContents (void)
{
	int i, j;
	int nfActorclip; /* number of faces with actorclip contentflag set */

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];
		side_t *side0;

		/* if the origin flag is set in the mapbrush_t struct, then the brushes
		 * work is done, and we can skip the mixed face contents check for this brush*/
		if (brush->contentFlags & CONTENTS_ORIGIN)
			continue;

		side0 = &brush->original_sides[0];
		nfActorclip = 0;

		CheckPropagateParserContentFlags(brush);

		for (j = 0; j < brush->numsides; j++) {
			side_t *side = &brush->original_sides[j];
			assert(side);

			nfActorclip += (side->contentFlags & CONTENTS_ACTORCLIP) ? 1 : 0;

			if (side0->contentFlags != side->contentFlags) {
				const int jNotZero = side->contentFlags & ~side0->contentFlags;
				const int zeroNotJ = side0->contentFlags & ~side->contentFlags;
				Check_Printf(VERB_CHECK, "  Brush %i (entity %i): mixed face contents (", brush->brushnum, brush->entitynum);
				if (jNotZero) {
					Check_Printf(VERB_CHECK, "face %i has and face 0 has not", j);
					DisplayContentFlags(jNotZero);
					if (zeroNotJ)
						Check_Printf(VERB_CHECK, ", ");
				}
				if (zeroNotJ) {
					Check_Printf(VERB_CHECK, "face 0 has and face %i has not", j);
					DisplayContentFlags(zeroNotJ);
				}
				Check_Printf(VERB_CHECK, ")\n");
			}
		}

		if (nfActorclip && nfActorclip <  brush->numsides / 2) {
			Check_Printf(VERB_CHECK, "* Brush %i (entity %i): ACTORCLIP set on less than half of the faces: removing.\n", brush->brushnum, brush->entitynum );
			for (j = 0; j < brush->numsides; j++) {
				side_t *side = &brush->original_sides[j];
				const ptrdiff_t index = side - brushsides;
				brush_texture_t *tex = &side_brushtextures[index];

				if (side->contentFlags & CONTENTS_ACTORCLIP && !Q_strcmp(tex->name, "tex_common/actorclip")) {
					Check_Printf(VERB_CHECK, "* Brush %i (entity %i): removing tex_common/actorclip, setting tex_common/error\n", brush->brushnum, brush->entitynum );
					Q_strncpyz(tex->name, "tex_common/error", sizeof(tex->name));
				}

				side->contentFlags &= ~CONTENTS_ACTORCLIP;
			}
		}
	}
}

void CheckBrushes (void)
{
	int i, j;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];

		Check_DuplicateBrushPlanes(brush);

		for (j = 0; j < brush->numsides; j++) {
			side_t *side = &brush->original_sides[j];
			const ptrdiff_t index = side - brushsides;
			brush_texture_t *tex = &side_brushtextures[index];

			assert(side);
			assert(tex);

#if 1
			/** @todo remove this once every map is run with ./ufo2map -fix brushes <map> */
			/* the old footstep value */
			if (side->contentFlags & 0x00040000) {
				side->contentFlags &= ~0x00040000;
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): converted old footstep content to new footstep surface value\n", brush->brushnum, brush->entitynum);
				side->surfaceFlags |= SURF_FOOTSTEP;
				tex->surfaceFlags |= SURF_FOOTSTEP;
			}
			/* the old fireaffected value */
			if (side->contentFlags & 0x0008) {
				side->contentFlags &= ~0x0008;
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): converted old fireaffected content to new fireaffected surface value\n", brush->brushnum, brush->entitynum);
				side->surfaceFlags |= SURF_BURN;
				tex->surfaceFlags |= SURF_BURN;
			}
#endif

			if (side->contentFlags & CONTENTS_ORIGIN && brush->entitynum == 0) {
				Check_Printf(VERB_CHECK, "* Brush %i (entity %i): origin brush inside worldspawn - removed CONTENTS_ORIGIN\n", brush->brushnum, brush->entitynum);
				side->contentFlags &= ~CONTENTS_ORIGIN;
			}
		}
	}
}
