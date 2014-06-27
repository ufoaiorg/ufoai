/**
 * @file
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


#include "map.h"
#include "bsp.h"
#include "textures.h"
#include "check/check.h"
#include "check/checkentities.h"
#include "common/aselib.h"
#include "../../shared/parse.h"

mapbrush_t mapbrushes[MAX_MAP_BRUSHES];
int nummapbrushes;

side_t brushsides[MAX_MAP_SIDES];
int nummapbrushsides;

brush_texture_t side_brushtextures[MAX_MAP_SIDES];

/** an index of the planes containing the faces of the brushes */
plane_t mapplanes[MAX_MAP_PLANES];
int nummapplanes;

#define	PLANE_HASHES	1024
static plane_t* planehash[PLANE_HASHES];

static int c_boxbevels = 0;
static int c_edgebevels = 0;

/*
=============================================================================
PLANE FINDING
=============================================================================
*/

/**
 * @brief Planes are stored by their distance (resp. the calculated
 * hash) in the plane hash table
 */
static inline int GetPlaneHashValueForDistance (const vec_t distance)
{
	int hash = (int)fabs(distance) * 27;
	hash &= (PLANE_HASHES - 1);
	return hash;
}

/**
 * @brief Set the type of the plane according to it's normal vector
 */
static int PlaneTypeForNormal (const vec3_t normal)
{
	vec_t ax, ay, az;

	/* NOTE: should these have an epsilon around 1.0? */
	if (normal[0] == 1.0 || normal[0] == -1.0)
		return PLANE_X;
	if (normal[1] == 1.0 || normal[1] == -1.0)
		return PLANE_Y;
	if (normal[2] == 1.0 || normal[2] == -1.0)
		return PLANE_Z;

	ax = fabs(normal[0]);
	ay = fabs(normal[1]);
	az = fabs(normal[2]);

	if (ax >= ay && ax >= az)
		return PLANE_ANYX;
	if (ay >= ax && ay >= az)
		return PLANE_ANYY;
	return PLANE_ANYZ;
}

/**
 * @brief Checks whether the given normal and distance vectors match the given plane by some epsilon value
 * @param p The plane to compare the normal and distance with
 * @param normal
 * @param dist
 * @return true if the normal and the distance vector are the same (by some epsilon) as in the given plane
 */
static inline bool PlaneEqual (const plane_t* p, const vec3_t normal, const vec_t dist)
{
	if (fabs(p->normal[0] - normal[0]) < NORMAL_EPSILON
	 && fabs(p->normal[1] - normal[1]) < NORMAL_EPSILON
	 && fabs(p->normal[2] - normal[2]) < NORMAL_EPSILON
	 && fabs(p->dist - dist) < MAP_DIST_EPSILON)
		return true;
	return false;
}

static inline void AddPlaneToHash (plane_t* p)
{
	const int hash = GetPlaneHashValueForDistance(p->dist);
	p->hash_chain = planehash[hash];
	planehash[hash] = p;
}

static uint16_t CreateNewFloatPlane (vec3_t normal, vec_t dist)
{
	plane_t* p;

	if (VectorLength(normal) < 0.5)
		Sys_Error("FloatPlane: bad normal (%.3f:%.3f:%.3f)", normal[0], normal[1], normal[2]);
	/* create a new plane */
	if (nummapplanes + 2 > MAX_MAP_PLANES)
		Sys_Error("MAX_MAP_PLANES (%i)", nummapplanes + 2);

	p = &mapplanes[nummapplanes];
	VectorCopy(normal, p->normal);
	p->dist = dist;
	p->type = (p + 1)->type = PlaneTypeForNormal(p->normal);

	VectorSubtract(vec3_origin, normal, (p + 1)->normal);
	(p + 1)->dist = -dist;

	nummapplanes += 2;

	/* always put axial planes facing positive first */
	if (AXIAL(p)) {
		if (p->normal[0] < 0 || p->normal[1] < 0 || p->normal[2] < 0) {
			/* flip order by swapping the planes */
			const plane_t temp = *p;
			*p = *(p + 1);
			*(p + 1) = temp;

			AddPlaneToHash(p);
			AddPlaneToHash(p + 1);
			return nummapplanes - 1;
		}
	}

	AddPlaneToHash(p);
	AddPlaneToHash(p + 1);
	return nummapplanes - 2;
}

/**
 * @brief Round the vector to int values
 * @param[in,out] normal the normal vector to snap
 * @note Can be used to save net bandwidth
 */
static inline bool SnapVector (vec3_t normal)
{
	for (int i = 0; i < 3; i++) {
		if (fabs(normal[i] - 1) < NORMAL_EPSILON) {
			VectorClear(normal);
			normal[i] = 1;
			return true;
		}
		if (fabs(normal[i] - -1) < NORMAL_EPSILON) {
			VectorClear(normal);
			normal[i] = -1;
			return true;
		}
	}
	return false;
}

/**
 * @brief Snaps normal to axis-aligned if it is within an epsilon of axial.
 * @param[in,out] normal - Plane normal vector (assumed to be unit length)
 * @param[in,out] dist - Plane constant - return as rounded to integer if it
 * is within an epsilon of @c MAP_DIST_EPSILON
 */
static inline void SnapPlane (vec3_t normal, vec_t* dist)
{
	SnapVector(normal);

	if (fabs(*dist - Q_rint(*dist)) < MAP_DIST_EPSILON)
		*dist = Q_rint(*dist);
}

uint16_t FindOrCreateFloatPlane (vec3_t normal, vec_t dist)
{
	int i;
	plane_t* p;
	int hash;

	SnapPlane(normal, &dist);
	hash = GetPlaneHashValueForDistance(dist);

	/* search the border bins as well */
	for (i = -1; i <= 1; i++) {
		const int h = (hash + i) & (PLANE_HASHES - 1);
		for (p = planehash[h]; p; p = p->hash_chain) {
			if (PlaneEqual(p, normal, dist)) {
				const intptr_t index = p - mapplanes;
				return (int16_t)index;
			}
		}
	}

	return CreateNewFloatPlane(normal, dist);
}

/**
 * @brief Builds a plane normal and distance from three points on the plane.
 * If the normal is nearly axial, it will be snapped to be axial. Looks
 * up the plane in the unique planes.
 * @param[in] b The brush that the points belong to.
 * @param[in] p0 Three points on the plane. (A vector with plane coordinates)
 * @param[in] p1 Three points on the plane. (A vector with plane coordinates)
 * @param[in] p2 Three points on the plane. (A vector with plane coordinates)
 * @return the index of the plane in the planes list.
 */
static int16_t PlaneFromPoints (const mapbrush_t* b, const vec3_t p0, const vec3_t p1, const vec3_t p2)
{
	vec3_t t1, t2, normal;
	vec_t dist;

	VectorSubtract(p0, p1, t1);
	VectorSubtract(p2, p1, t2);
	CrossProduct(t1, t2, normal);
	VectorNormalize(normal);

	dist = DotProduct(p0, normal);

	if (!VectorNotEmpty(normal))
		Sys_Error("PlaneFromPoints: Bad normal (null) for brush %i", b->brushnum);

	return FindOrCreateFloatPlane(normal, dist);
}


/*==================================================================== */


/**
 * @brief Get the content flags for a given brush
 * @param b The mapbrush to get the content flags for
 * @return The calculated content flags
 */
static int BrushContents (mapbrush_t* b)
{
	int contentFlags, i;
	const side_t* s;

	s = &b->original_sides[0];
	contentFlags = s->contentFlags;
	for (i = 1; i < b->numsides; i++, s++) {
		if (s->contentFlags != contentFlags) {
			Verb_Printf(VERB_EXTRA, "Entity %i, Brush %i: mixed face contents (f: %i, %i)\n"
				, b->entitynum, b->brushnum, s->contentFlags, contentFlags);
			break;
		}
	}

	return contentFlags;
}

/**
 * @brief Extract the level flags (1-8) from the content flags of the given brush
 * @param brush The brush to extract the level flags from
 * @return The level flags (content flags) of the given brush
 */
byte GetLevelFlagsFromBrush (const mapbrush_t* brush)
{
	const byte levelflags = (brush->contentFlags >> 8) & 0xFF;
	return levelflags;
}

/*============================================================================ */

/**
 * @brief Adds any additional planes necessary to allow the brush to be expanded
 * against axial bounding boxes
 */
static void AddBrushBevels (mapbrush_t* b)
{
	int axis, dir;
	int i, l, order;
	vec3_t normal;

	/* add the axial planes */
	order = 0;
	for (axis = 0; axis < 3; axis++) {
		for (dir = -1; dir <= 1; dir += 2, order++) {
			side_t* s;
			/* see if the plane is already present */
			for (i = 0, s = b->original_sides; i < b->numsides; i++, s++) {
				if (mapplanes[s->planenum].normal[axis] == dir)
					break;
			}

			if (i == b->numsides) {	/* add a new side */
				float dist;
				if (nummapbrushsides == MAX_MAP_BRUSHSIDES)
					Sys_Error("MAX_MAP_BRUSHSIDES (%i)", nummapbrushsides);
				nummapbrushsides++;
				b->numsides++;
				VectorClear(normal);
				normal[axis] = dir;
				if (dir == 1)
					dist = b->mbBox.maxs[axis];
				else
					dist = -b->mbBox.mins[axis];
				s->planenum = FindOrCreateFloatPlane(normal, dist);
				s->texinfo = b->original_sides[0].texinfo;
				s->contentFlags = b->original_sides[0].contentFlags;
				s->bevel = true;
				c_boxbevels++;
			}

			/* if the plane is not in it canonical order, swap it */
			if (i != order) {
				const ptrdiff_t index = b->original_sides - brushsides;
				side_t sidetemp = b->original_sides[order];
				brush_texture_t tdtemp = side_brushtextures[index + order];

				b->original_sides[order] = b->original_sides[i];
				b->original_sides[i] = sidetemp;

				side_brushtextures[index + order] = side_brushtextures[index + i];
				side_brushtextures[index + i] = tdtemp;
			}
		}
	}

	/* add the edge bevels */
	if (b->numsides == 6)
		return;		/* pure axial */

	/* test the non-axial plane edges */
	for (i = 6; i < b->numsides; i++) {
		side_t* s = b->original_sides + i;
		winding_t* w = s->winding;
		if (!w)
			continue;

		for (int j = 0; j < w->numpoints; j++) {
			int k = (j + 1) % w->numpoints;
			vec3_t vec;

			VectorSubtract(w->p[j], w->p[k], vec);
			if (VectorNormalize(vec) < 0.5)
				continue;

			SnapVector(vec);

			for (k = 0; k < 3; k++)
				if (vec[k] == -1 || vec[k] == 1
				 || (vec[k] == 0.0f && vec[(k + 1) % 3] == 0.0f))
					break;	/* axial */
			if (k != 3)
				continue;	/* only test non-axial edges */

			/* try the six possible slanted axials from this edge */
			for (axis = 0; axis < 3; axis++) {
				for (dir = -1; dir <= 1; dir += 2) {
					/* construct a plane */
					vec3_t vec2 = {0, 0, 0};
					float dist;
					side_t* s2;

					vec2[axis] = dir;
					CrossProduct(vec, vec2, normal);
					if (VectorNormalize(normal) < 0.5)
						continue;
					dist = DotProduct(w->p[j], normal);

					/* if all the points on all the sides are
					 * behind this plane, it is a proper edge bevel */
					for (k = 0; k < b->numsides; k++) {
						winding_t* w2;
						float minBack;

						/* @note: This leads to different results on different archs
						 * due to float rounding/precision errors - use the -ffloat-store
						 * feature of gcc to 'fix' this */
						/* if this plane has already been used, skip it */
						if (PlaneEqual(&mapplanes[b->original_sides[k].planenum], normal, dist))
							break;

						w2 = b->original_sides[k].winding;
						if (!w2)
							continue;
						minBack = 0.0f;
						for (l = 0; l < w2->numpoints; l++) {
							const float d = DotProduct(w2->p[l], normal) - dist;
							if (d > 0.1)
								break;	/* point in front */
							if (d < minBack)
								minBack = d;
						}
						/* if some point was at the front */
						if (l != w2->numpoints)
							break;
						/* if no points at the back then the winding is on the bevel plane */
						if (minBack > -0.1f)
							break;
					}

					if (k != b->numsides)
						continue;	/* wasn't part of the outer hull */
					/* add this plane */
					if (nummapbrushsides == MAX_MAP_BRUSHSIDES)
						Sys_Error("MAX_MAP_BRUSHSIDES (%i)", nummapbrushsides);
					nummapbrushsides++;
					s2 = &b->original_sides[b->numsides];
					s2->planenum = FindOrCreateFloatPlane(normal, dist);
					s2->texinfo = b->original_sides[0].texinfo;
					s2->contentFlags = b->original_sides[0].contentFlags;
					s2->bevel = true;
					c_edgebevels++;
					b->numsides++;
				}
			}
		}
	}
}

/**
 * @brief makes basewindings for sides and mins / maxs for the brush
 */
static bool MakeBrushWindings (mapbrush_t* brush)
{
	int i, j;

	brush->mbBox.setNegativeVolume();

	for (i = 0; i < brush->numsides; i++) {
		const plane_t* plane = &mapplanes[brush->original_sides[i].planenum];
		winding_t* w = BaseWindingForPlane(plane->normal, plane->dist);
		for (j = 0; j < brush->numsides && w; j++) {
			if (i == j)
				continue;
			/* back side clipaway */
			if (brush->original_sides[j].planenum == (brush->original_sides[j].planenum ^ 1))
				continue;
			if (brush->original_sides[j].bevel)
				continue;
			plane = &mapplanes[brush->original_sides[j].planenum ^ 1];
			ChopWindingInPlace(&w, plane->normal, plane->dist, 0); /*CLIP_EPSILON); */
		}

		side_t* side = &brush->original_sides[i];
		side->winding = w;
		if (w) {
			side->visible = true;
			for (j = 0; j < w->numpoints; j++)
				brush->mbBox.add(w->p[j]);
		}
	}

	for (i = 0; i < 3; i++) {
		if (brush->mbBox.mins[i] < -MAX_WORLD_WIDTH || brush->mbBox.maxs[i] > MAX_WORLD_WIDTH)
			Com_Printf("entity %i, brush %i: bounds out of world range (%f:%f)\n",
				brush->entitynum, brush->brushnum, brush->mbBox.mins[i], brush->mbBox.maxs[i]);
		if (brush->mbBox.mins[i] > MAX_WORLD_WIDTH || brush->mbBox.maxs[i] < -MAX_WORLD_WIDTH) {
			Com_Printf("entity %i, brush %i: no visible sides on brush\n", brush->entitynum, brush->brushnum);
			brush->mbBox.reset();
		}
	}

	return true;
}

/**
 * @brief Checks whether the surface and content flags are correct
 * @sa ParseBrush
 * @sa SetImpliedFlags
 */
static inline void CheckFlags (side_t* side, const mapbrush_t* b)
{
	if ((side->contentFlags & CONTENTS_ACTORCLIP) && (side->contentFlags & CONTENTS_PASSABLE))
		Sys_Error("Brush %i (entity %i) has invalid mix of passable and actorclip", b->brushnum, b->entitynum);
	if ((side->contentFlags & (CONTENTS_LIGHTCLIP | CONTENTS_ACTORCLIP | CONTENTS_WEAPONCLIP)) && (side->contentFlags & CONTENTS_SOLID))
		Sys_Error("Brush %i (entity %i) has invalid mix of clips and solid flags", b->brushnum, b->entitynum);
}

/** @brief How many materials were created for this map */
static int materialsCnt = 0;

/**
 * @brief Generates material files in case the settings can be guessed from map file
 */
static void GenerateMaterialFile (const char* filename, int mipTexIndex, side_t* s)
{
	bool terrainByTexture = false;
	char fileBase[MAX_OSPATH], materialPath[MAX_OSPATH];

	if (!config.generateMaterialFile)
		return;

	/* we already have a material definition for this texture */
	if (textureref[mipTexIndex].materialMarked)
		return;

	assert(filename);

	Com_StripExtension(filename, fileBase, sizeof(fileBase));
	Com_sprintf(materialPath, sizeof(materialPath), "materials/%s.mat", Com_SkipPath(fileBase));

	ScopedFile f;
	FS_OpenFile(materialPath, &f, FILE_APPEND);
	if (!f) {
		Com_Printf("Could not open material file '%s' for writing\n", materialPath);
		config.generateMaterialFile = false;
		return;
	}

	if (strstr(textureref[mipTexIndex].name, "dirt")
	 || strstr(textureref[mipTexIndex].name, "rock")
	 || strstr(textureref[mipTexIndex].name, "grass")) {
		terrainByTexture = true;
	}

	if ((s->contentFlags & CONTENTS_TERRAIN) || terrainByTexture) {
		FS_Printf(&f, "{\n\tmaterial %s\n\t{\n\t\ttexture <fillme>\n\t\tterrain 0 64\n\t\tlightmap\n\t}\n}\n", textureref[mipTexIndex].name);
		textureref[mipTexIndex].materialMarked = true;
		materialsCnt++;
	}

	/* envmap for water surfaces */
	if ((s->contentFlags & CONTENTS_WATER)
	 || strstr(textureref[mipTexIndex].name, "glass")
	 || strstr(textureref[mipTexIndex].name, "window")) {
		FS_Printf(&f, "{\n\tmaterial %s\n\tspecular 2.0\n\t{\n\t\tenvmap 0\n\t}\n}\n", textureref[mipTexIndex].name);
		textureref[mipTexIndex].materialMarked = true;
		materialsCnt++;
	}

	if (strstr(textureref[mipTexIndex].name, "wood")) {
		FS_Printf(&f, "{\n\tmaterial %s\n\tspecular 0.2\n}\n", textureref[mipTexIndex].name);
		textureref[mipTexIndex].materialMarked = true;
		materialsCnt++;
	}

	if (strstr(textureref[mipTexIndex].name, "wall")) {
		FS_Printf(&f, "{\n\tmaterial %s\n\tspecular 0.6\n\tbump 2.0\n}\n", textureref[mipTexIndex].name);
		textureref[mipTexIndex].materialMarked = true;
		materialsCnt++;
	}
}

/** @brief Amount of footstep surfaces found in this map */
static int footstepsCnt = 0;

/**
 * @brief Generate a list of textures that should have footsteps when walking on them
 * @param[in] filename Add this texture to the list of
 * textures where we should have footstep sounds for
 * @param[in] mipTexIndex The index in the textures array
 * @sa SV_GetFootstepSound
 * @sa Com_GetTerrainType
 */
static void GenerateFootstepList (const char* filename, int mipTexIndex)
{
	if (!config.generateFootstepFile)
		return;

	if (textureref[mipTexIndex].footstepMarked)
		return;

	assert(filename);

	char fileBase[MAX_OSPATH];
	Com_StripExtension(filename, fileBase, sizeof(fileBase));

	ScopedFile f;
	FS_OpenFile(va("%s.footsteps", fileBase), &f, FILE_APPEND);
	if (!f) {
		Com_Printf("Could not open footstep file '%s.footsteps' for writing\n", fileBase);
		config.generateFootstepFile = false;
		return;
	}
#ifdef _WIN32
	FS_Printf(&f, "terrain %s {\n}\n\n", textureref[mipTexIndex].name);
#else
	FS_Printf(&f, "%s\n", textureref[mipTexIndex].name);
#endif
	footstepsCnt++;
	textureref[mipTexIndex].footstepMarked = true;
}

/**
 * @brief Parses a brush from the map file
 * @sa FindMiptex
 * @param[in] mapent The entity the brush to parse belongs to
 * @param[in] filename The map filename, used to derive the name for the footsteps file
 */
static void ParseBrush (entity_t* mapent, const char* filename)
{
	int j, k;
	brush_texture_t td;
	vec3_t planepts[3];
	const int checkOrFix = config.performMapCheck || config.fixMap ;

	if (nummapbrushes == MAX_MAP_BRUSHES)
		Sys_Error("nummapbrushes == MAX_MAP_BRUSHES (%i)", nummapbrushes);

	mapbrush_t* b = &mapbrushes[nummapbrushes];
	OBJZERO(*b);
	b->original_sides = &brushsides[nummapbrushsides];
	b->entitynum = num_entities - 1;
	b->brushnum = nummapbrushes - mapent->firstbrush;

	do {
		if (Q_strnull(GetToken()))
			break;
		if (*parsedToken == '}')
			break;

		if (nummapbrushsides == MAX_MAP_BRUSHSIDES)
			Sys_Error("nummapbrushsides == MAX_MAP_BRUSHSIDES (%i)", nummapbrushsides);
		side_t* side = &brushsides[nummapbrushsides];

		/* read the three point plane definition */
		for (int i = 0; i < 3; i++) {
			if (i != 0)
				GetToken();
			if (*parsedToken != '(')
				Sys_Error("parsing brush");

			for (j = 0; j < 3; j++) {
				GetToken();
				planepts[i][j] = atof(parsedToken);
			}

			GetToken();
			if (*parsedToken != ')')
				Sys_Error("parsing brush");
		}

		/* read the texturedef */
		GetToken();
		if (strlen(parsedToken) >= MAX_TEXPATH) {
			if (config.performMapCheck || config.fixMap)
				Com_Printf("  ");/* hack to make this look like output from Check_Printf() */
			Com_Printf("ParseBrush: texture name too long (limit %i): %s\n", MAX_TEXPATH, parsedToken);
			if (config.fixMap)
				Sys_Error("Aborting, as -fix is active and saving might corrupt *.map by truncating texture name");
		}
		Q_strncpyz(td.name, parsedToken, sizeof(td.name));

		td.shift[0] = atof(GetToken());
		td.shift[1] = atof(GetToken());
		td.rotate = atof(GetToken());
		td.scale[0] = atof(GetToken());
		td.scale[1] = atof(GetToken());

		/* find default flags and values */
		const int mt = FindMiptex(td.name);
		side->surfaceFlags = td.surfaceFlags = side->contentFlags = td.value = 0;

		if (TokenAvailable()) {
			side->contentFlags = atoi(GetToken());
			side->surfaceFlags = td.surfaceFlags = atoi(GetToken());
			td.value = atoi(GetToken());
		}

		/* if in check or fix mode, let them choose to do this (with command line options),
		 * and then call is made elsewhere */
		if (!checkOrFix) {
			SetImpliedFlags(side, &td, b);
			/* if no other content flags are set - make this solid */
			if (!checkOrFix && side->contentFlags == 0)
				side->contentFlags = CONTENTS_SOLID;
		}

		/* translucent objects are automatically classified as detail and window */
		if (side->surfaceFlags & (SURF_BLEND33 | SURF_BLEND66 | SURF_ALPHATEST)) {
			side->contentFlags |= CONTENTS_DETAIL;
			side->contentFlags |= CONTENTS_TRANSLUCENT;
			side->contentFlags |= CONTENTS_WINDOW;
			side->contentFlags &= ~CONTENTS_SOLID;
		}
		if (config.fulldetail)
			side->contentFlags &= ~CONTENTS_DETAIL;
		if (!checkOrFix) {
			if (!(side->contentFlags & ((LAST_VISIBLE_CONTENTS - 1)
				| CONTENTS_ACTORCLIP | CONTENTS_WEAPONCLIP | CONTENTS_LIGHTCLIP)))
				side->contentFlags |= CONTENTS_SOLID;

			/* hints and skips are never detail, and have no content */
			if (side->surfaceFlags & (SURF_HINT | SURF_SKIP)) {
				side->contentFlags = 0;
				side->surfaceFlags &= ~CONTENTS_DETAIL;
			}
		}

		/* check whether the flags are ok */
		CheckFlags(side, b);

		/* generate a list of textures that should have footsteps when walking on them */
		if (mt > 0 && (side->surfaceFlags & SURF_FOOTSTEP))
			GenerateFootstepList(filename, mt);
		GenerateMaterialFile(filename, mt, side);

		/* find the plane number */
		int planenum = PlaneFromPoints(b, planepts[0], planepts[1], planepts[2]);
		if (planenum == PLANENUM_LEAF) {
			Com_Printf("Entity %i, Brush %i: plane with no normal\n", b->entitynum, b->brushnum);
			continue;
		}

		for (j = 0; j < 3; j++)
			VectorCopy(planepts[j], mapplanes[planenum].planeVector[j]);

		/* see if the plane has been used already */
		for (k = 0; k < b->numsides; k++) {
			const side_t* s2 = b->original_sides + k;
			if (s2->planenum == planenum) {
				Com_Printf("Entity %i, Brush %i: duplicate plane\n", b->entitynum, b->brushnum);
				break;
			}
			if (s2->planenum == (planenum ^ 1)) {
				Com_Printf("Entity %i, Brush %i: mirrored plane\n", b->entitynum, b->brushnum);
				break;
			}
		}
		if (k != b->numsides)
			continue;		/* duplicated */

		/* keep this side */
		side = b->original_sides + b->numsides;
		side->planenum = planenum;
		side->texinfo = TexinfoForBrushTexture(&mapplanes[planenum],
			&td, vec3_origin, side->contentFlags & CONTENTS_TERRAIN);
		side->brush = b;

		/* save the td off in case there is an origin brush and we
		 * have to recalculate the texinfo */
		side_brushtextures[nummapbrushsides] = td;

		Verb_Printf(VERB_DUMP, "Brush %i Side %i (%f %f %f) (%f %f %f) (%f %f %f) texinfo:%i[%s] plane:%i\n", nummapbrushes, nummapbrushsides,
			planepts[0][0], planepts[0][1], planepts[0][2],
			planepts[1][0], planepts[1][1], planepts[1][2],
			planepts[2][0], planepts[2][1], planepts[2][2],
			side->texinfo, td.name, planenum);

		nummapbrushsides++;
		b->numsides++;
	} while (1);

	/* get the content for the entire brush */
	b->contentFlags = BrushContents(b);

	/* copy all set face contentflags to the brush contentflags */
	for (int m = 0; m < b->numsides; m++)
		b->contentFlags |= b->original_sides[m].contentFlags;

	/* set DETAIL, TRANSLUCENT contentflags on all faces, if they have been set on any.
	 * called separately, if in check/fix mode */
	if (!checkOrFix)
		CheckPropagateParserContentFlags(b);

	/* allow detail brushes to be removed */
	if (config.nodetail && (b->contentFlags & CONTENTS_DETAIL)) {
		b->numsides = 0;
		return;
	}

	/* allow water brushes to be removed */
	if (config.nowater && (b->contentFlags & CONTENTS_WATER)) {
		b->numsides = 0;
		return;
	}

	/* create windings for sides and bounds for brush */
	MakeBrushWindings(b);

	Verb_Printf(VERB_DUMP, "Brush %i mins (%f %f %f) maxs (%f %f %f)\n", nummapbrushes,
		b->mbBox.mins[0], b->mbBox.mins[1], b->mbBox.mins[2],
		b->mbBox.maxs[0], b->mbBox.maxs[1], b->mbBox.maxs[2]);

	/* origin brushes are removed, but they set
	 * the rotation origin for the rest of the brushes (like func_door)
	 * in the entity. After the entire entity is parsed, the planenums
	 * and texinfos will be adjusted for the origin brush */
	if (!checkOrFix && (b->contentFlags & CONTENTS_ORIGIN)) {
		char string[32];
		vec3_t origin;

		if (num_entities == 1) {
			Sys_Error("Entity %i, Brush %i: origin brushes not allowed in world"
				, b->entitynum, b->brushnum);
			return;
		}

		b->mbBox.getCenter(origin);

		Com_sprintf(string, sizeof(string), "%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2]);
		SetKeyValue(&entities[b->entitynum], "origin", string);
		Verb_Printf(VERB_EXTRA, "Entity %i, Brush %i: set origin to %s\n", b->entitynum, b->brushnum, string);

		VectorCopy(origin, entities[b->entitynum].origin);

		/* don't keep this brush */
		b->numsides = 0;

		return;
	}

	if (!checkOrFix)
		AddBrushBevels(b);

	nummapbrushes++;
	mapent->numbrushes++;
}

/**
 * @brief Takes all of the brushes from the current entity and adds them to the world's brush list.
 * @note Used by func_group
 * @note This will only work if the func_group is the last entity currently known. At the moment,
 * this function is only called by ParseMapEntity and this happens directly after the func_group
 * is parsed, so this is OK.
 * @sa MoveModelToWorld
 */
static void MoveBrushesToWorld (entity_t* mapent)
{
	int newbrushes, worldbrushes, i;
	mapbrush_t* temp;

	/* this is pretty gross, because the brushes are expected to be
	 * in linear order for each entity */

	newbrushes = mapent->numbrushes;
	worldbrushes = entities[0].numbrushes;

	if (newbrushes == 0)
		Sys_Error("Empty func_group - clean your map");

	temp = Mem_AllocTypeN(mapbrush_t, newbrushes);
	memcpy(temp, mapbrushes + mapent->firstbrush, newbrushes * sizeof(*temp));

	/* make space to move the brushes (overlapped copy) */
	memmove(mapbrushes + worldbrushes + newbrushes,
		mapbrushes + worldbrushes,
		sizeof(mapbrush_t) * (nummapbrushes - worldbrushes - newbrushes));

	/* copy the new brushes down */
	memcpy(mapbrushes + worldbrushes, temp, sizeof(*temp) * newbrushes);

	/* fix up indexes */
	entities[0].numbrushes += newbrushes;
	for (i = 1; i < num_entities; i++)
		entities[i].firstbrush += newbrushes;
	Mem_Free(temp);

	mapent->numbrushes = 0;
}

/**
 * @brief If there was an origin brush, offset all of the planes and texinfo
 * @note Used for e.g. func_door or func_rotating
 */
static void AdjustBrushesForOrigin (const entity_t* ent)
{
	for (int i = 0; i < ent->numbrushes; i++) {
		mapbrush_t* b = &mapbrushes[ent->firstbrush + i];
		for (int j = 0; j < b->numsides; j++) {
			side_t* s = &b->original_sides[j];
			const ptrdiff_t index = s - brushsides;
			const vec_t newdist = mapplanes[s->planenum].dist -
				DotProduct(mapplanes[s->planenum].normal, ent->origin);
			s->surfaceFlags |= SURF_ORIGIN;
			side_brushtextures[index].surfaceFlags |= SURF_ORIGIN;
			s->planenum = FindOrCreateFloatPlane(mapplanes[s->planenum].normal, newdist);
			s->texinfo = TexinfoForBrushTexture(&mapplanes[s->planenum],
				&side_brushtextures[index], ent->origin, s->contentFlags & CONTENTS_TERRAIN);
			s->brush = b;
		}
		/* create windings for sides and bounds for brush */
		MakeBrushWindings(b);
	}
}

/**
 * @brief Checks whether this entity is an inline model that should have brushes
 * @param[in] entName
 * @return true if the name of the entity implies, that this is an inline model
 */
static inline bool IsInlineModelEntity (const char* entName)
{
	const bool inlineModelEntity = (Q_streq("func_breakable", entName)
			|| Q_streq("func_door", entName)
			|| Q_streq("func_door_sliding", entName)
			|| Q_streq("func_rotating", entName)
			|| Q_strstart(entName, "trigger_"));
	return inlineModelEntity;
}

/**
 * @brief Searches the entities array for an entity with the parameter targetname
 * that matches the searched target parameter
 * @param[in] target The targetname value that the entity should have that we are
 * looking for
 */
entity_t* FindTargetEntity (const char* target)
{
	for (int i = 0; i < num_entities; i++) {
		const char* n = ValueForKey(&entities[i], "targetname");
		if (Q_streq(n, target))
			return &entities[i];
	}

	return nullptr;
}

/**
 * @brief Parsed map entites and brushes
 * @sa ParseBrush
 * @param[in] filename The map filename
 * @param[in] entityString The body of the entity we are parsing
 */
static bool ParseMapEntity (const char* filename, const char* entityString)
{
	entity_t* mapent;
	const char* entName;
	static int worldspawnCount = 0;
	int notCheckOrFix = !(config.performMapCheck || config.fixMap);

	if (Q_strnull(GetToken()))
		return false;

	if (*parsedToken != '{')
		Sys_Error("ParseMapEntity: { not found");

	if (num_entities == MAX_MAP_ENTITIES)
		Sys_Error("num_entities == MAX_MAP_ENTITIES (%i)", num_entities);

	mapent = &entities[num_entities++];
	OBJZERO(*mapent);
	mapent->firstbrush = nummapbrushes;
	mapent->numbrushes = 0;

	do {
		if (Q_strnull(GetToken()))
			Sys_Error("ParseMapEntity: EOF without closing brace");
		if (*parsedToken == '}')
			break;
		if (*parsedToken == '{')
			ParseBrush(mapent, filename);
		else {
			epair_t* e = ParseEpair(num_entities);
			e->next = mapent->epairs;
			mapent->epairs = e;
		}
	} while (true);

	GetVectorForKey(mapent, "origin", mapent->origin);

	entName = ValueForKey(mapent, "classname");

	/* offset all of the planes and texinfo if needed */
	if (IsInlineModelEntity(entName) && VectorNotEmpty(mapent->origin))
		AdjustBrushesForOrigin(mapent);

	if (num_entities == 1 && !Q_streq("worldspawn", entName))
		Sys_Error("The first entity must be worldspawn, it is: %s", entName);
	if (notCheckOrFix && Q_streq("func_group", entName)) {
		/* group entities are just for editor convenience
		 * toss all brushes into the world entity */
		MoveBrushesToWorld(mapent);
		num_entities--;
	} else if (IsInlineModelEntity(entName)) {
		if (mapent->numbrushes == 0 && notCheckOrFix) {
			Com_Printf("Warning: %s has no brushes assigned (entnum: %i)\n", entName, num_entities);
			num_entities--;
		}
	} else if (Q_streq("worldspawn", entName)) {
		worldspawnCount++;
		if (worldspawnCount > 1)
			Com_Printf("Warning: more than one %s in one map\n", entName);

		const char* text = entityString;
		do {
			const char* token = Com_Parse(&text);
			if (Q_strnull(token))
				break;
			const char* key = Mem_StrDup(token);
			token = Com_Parse(&text);
			if (Q_strnull(token))
				break;
			const char* value = Mem_StrDup(token);
			epair_t* e = AddEpair(key, value, num_entities);
			if (!config.fixMap || !EpairCheckForDuplicate(mapent, e)) {
				e->next = mapent->epairs;
				e->ump = true;
				mapent->epairs = e;
			}
		} while (true);
	}
	return true;
}

/**
 * @brief Recurse down the epair list
 * @note First writes the last element
 */
static inline void WriteMapEntities (qFILE* f, const epair_t* e)
{
	if (!e)
		return;

	if (e->next)
		WriteMapEntities(f, e->next);

	if (!e->ump)
		FS_Printf(f, "\"%s\" \"%s\"\n", e->key, e->value);
}


/**
 * @brief write a brush to the .map file
 * @param[in] brush The brush to write
 * @param[in] j the index of the brush in the entity, to label the brush in the comment in the map file
 * @param[in] f file to write to
 */
static void WriteMapBrush (const mapbrush_t* brush, const int j, qFILE* f)
{
	FS_Printf(f, "// brush %i\n{\n", j);
	for (int k = 0; k < brush->numsides; k++) {
		const side_t* side = &brush->original_sides[k];
		const ptrdiff_t index = side - brushsides;
		const brush_texture_t* t = &side_brushtextures[index];
		const plane_t* p = &mapplanes[side->planenum];

		for (int i = 0; i < 3; i++)
			FS_Printf(f, "( %.7g %.7g %.7g ) ", p->planeVector[i][0], p->planeVector[i][1], p->planeVector[i][2]);
		FS_Printf(f, "%s ", t->name);
		FS_Printf(f, "%.7g %.7g %.7g ", t->shift[0], t->shift[1], t->rotate);
		FS_Printf(f, "%.7g %.7g ", t->scale[0], t->scale[1]);
		FS_Printf(f, "%i %i %i\n", side->contentFlags, t->surfaceFlags, t->value);
	}
	FS_Printf(f, "}\n");
}

/**
 * @sa LoadMapFile
 * @sa FixErrors
 */
void WriteMapFile (const char* filename)
{
	int removed;

	Verb_Printf(VERB_NORMAL, "writing map: '%s'\n", filename);

	ScopedFile f;
	FS_OpenFile(filename, &f, FILE_WRITE);
	if (!f)
		Sys_Error("Could not open %s for writing", filename);

	removed = 0;
	for (int i = 0; i < num_entities; i++) {
		const entity_t* mapent = &entities[i];
		const epair_t* e = mapent->epairs;

		/* maybe we don't want to write it back into the file */
		if (mapent->skip) {
			removed++;
			continue;
		}
		FS_Printf(&f, "// entity %i\n{\n", i - removed);
		WriteMapEntities(&f, e);

		/* need 2 counters. j counts the brushes in the source entity.
		 * jc counts the brushes written back. they may differ if some are skipped,
		 * eg they are microbrushes */
		int j, jc;
		for (j = 0, jc = 0; j < mapent->numbrushes; j++) {
			const mapbrush_t* brush = &mapbrushes[mapent->firstbrush + j];
			if (brush->skipWriteBack)
				continue;
			WriteMapBrush(brush, jc++, &f);
		}

		/* add brushes from func_groups with single members to worldspawn */
		if (i == 0) {
			int numToAdd;
			mapbrush_t** brushesToAdd = Check_ExtraBrushesForWorldspawn(&numToAdd);
			if (brushesToAdd != nullptr) {
				for (int k = 0; k < numToAdd; k++) {
					if (brushesToAdd[k]->skipWriteBack)
						continue;
					WriteMapBrush(brushesToAdd[k], j++, &f);
				}
				Mem_Free(brushesToAdd);
			}
		}
		FS_Printf(&f, "}\n");
	}

	if (removed)
		Verb_Printf(VERB_NORMAL, "removed %i entities\n", removed);
}

/**
 * @brief Parses an ump file that contains the random map definition
 * @param[in] name The basename of the ump file (without extension)
 * @param[in] inherit When @c true, this is called to inherit tile definitions
 * @param[out] entityString An entity string that is used for all map tiles. Parsed from the ump.
 * from another ump file (no assemblies)
 */
static void ParseUMP (const char* name, char* entityString, bool inherit)
{
	char filename[MAX_QPATH];
	byte* buf;
	const char* text;

	*entityString = '\0';

	/* load the map info */
	Com_sprintf(filename, sizeof(filename), "maps/%s.ump", name);
	FS_LoadFile(filename, &buf);
	if (!buf)
		return;

	/* parse it */
	text = (const char*)buf;
	do {
		const char* token = Com_Parse(&text);
		if (!text)
			break;

		if (Q_streq(token, "extends")) {
			token = Com_Parse(&text);
			if (inherit)
				Com_Printf("ParseUMP: Too many extends in %s 'extends %s' ignored\n", filename, token);
			else
				ParseUMP(token, entityString, true);
		} else if (Q_streq(token, "worldspawn")) {
			const char* start = nullptr;
			const int length = Com_GetBlock(&text, &start);
			if (length == -1) {
				Com_Printf("ParseUMP: Not a valid worldspawn block in '%s'\n", filename);
			} else {
				if (length >= MAX_TOKEN_CHARS) {
					Com_Printf("worldspawn is too big - only %i characters are allowed", MAX_TOKEN_CHARS);
				} else if (length == 0) {
					Com_Printf("no worldspawn settings in ump file\n");
				} else {
					Q_strncpyz(entityString, start, length);
					Com_Printf("use worldspawn settings from ump file\n");
				}
			}
		} else if (token[0] == '{') {
			/* ignore unknown block */
			text = strchr(text, '}') + 1;
			if (!text)
				break;
		}
	} while (text);

	/* free the file */
	FS_FreeFile(buf);
}

/**
 * @brief The contract is that the directory name is equal to the base of the ump filename
 */
static const char* GetUMPName (const char* mapFilename)
{
	static char name[MAX_QPATH];
	const char* filename = Com_SkipPath(mapFilename);
	/* if we are in no subdir, we don't have any ump */
	if (filename == nullptr)
		return nullptr;

	const char* mapsDir = "maps/";
	const int lMaps = strlen(mapsDir);
	const int l = strlen(filename);
	const int targetLength = strlen(mapFilename) - lMaps - l;
	if (targetLength <= 0)
		return nullptr;
	Q_strncpyz(name, mapFilename + lMaps, targetLength);
	Com_Printf("...ump: '%s%s.ump'\n", mapsDir, name);
	return name;
}

/**
 * @sa WriteMapFile
 * @sa ParseMapEntity
 */
void LoadMapFile (const char* filename)
{
	Verb_Printf(VERB_EXTRA, "--- LoadMapFile ---\n");

	LoadScriptFile(filename);

	OBJZERO(mapbrushes);
	nummapbrushes = 0;

	OBJZERO(brushsides);
	nummapbrushsides = 0;

	OBJZERO(side_brushtextures);

	OBJZERO(mapplanes);

	num_entities = 0;
	/* Create this shortcut to mapTiles[0] */
	curTile = &mapTiles.mapTiles[0];
	/* Set the number of tiles to 1. This is fix for ufo2map right now. */
	mapTiles.numTiles = 1;

	char entityString[2 * MAX_TOKEN_CHARS];
	const char* ump = GetUMPName(filename);
	if (ump != nullptr)
		ParseUMP(ump, entityString, false);

	while (ParseMapEntity(filename, entityString));

	int subdivide = atoi(ValueForKey(&entities[0], "subdivide"));
	if (subdivide >= 256 && subdivide <= 2048) {
		Verb_Printf(VERB_EXTRA, "Using subdivide %d from worldspawn.\n", subdivide);
		config.subdivideSize = subdivide;
	}

	if (footstepsCnt)
		Com_Printf("Generated footstep file with %i entries\n", footstepsCnt);
	if (materialsCnt)
		Com_Printf("Generated material file with %i entries\n", materialsCnt);

	AABB mapBox;
	mapBox.setNegativeVolume();
	for (int i = 0; i < entities[0].numbrushes; i++) {
		if (mapbrushes[i].mbBox.mins[0] > MAX_WORLD_WIDTH)
			continue;	/* no valid points */
		mapBox.add(mapbrushes[i].mbBox);
	}

	/* save a copy of the brushes */
	memcpy(mapbrushes + nummapbrushes, mapbrushes, sizeof(mapbrush_t) * nummapbrushes);

	Verb_Printf(VERB_EXTRA, "%5i brushes\n", nummapbrushes);
	Verb_Printf(VERB_EXTRA, "%5i total sides\n", nummapbrushsides);
	Verb_Printf(VERB_EXTRA, "%5i boxbevels\n", c_boxbevels);
	Verb_Printf(VERB_EXTRA, "%5i edgebevels\n", c_edgebevels);
	Verb_Printf(VERB_EXTRA, "%5i entities\n", num_entities);
	Verb_Printf(VERB_EXTRA, "%5i planes\n", nummapplanes);
	char sizeStr[AABB_STRING];
	mapBox.asIntString(sizeStr, sizeof(sizeStr));
	Verb_Printf(VERB_EXTRA, "size: %s\n", sizeStr);
}
