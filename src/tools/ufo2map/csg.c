/**
 * @file csg.c
 * @brief Constructive Solids Geometry
 * @note tag all brushes with original contents
 * brushes may contain multiple contents
 * there will be no brush overlap after csg phase
 *
 * each side has a count of the other sides it splits
 *
 * the best split will be the one that minimizes the total split counts
 * of all remaining sides
 *
 * precalc side on plane table
 *
 * evaluate split side
 * @code
 * {
 * cost = 0
 * for all sides
 *   for all sides
 *     get
 *       if side splits side and splitside is on same child
 *         cost++;
 * }
 * @endcode
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

#include "bsp.h"

/**
 * @return a list of brushes that remain after B is subtracted from A.
 * @note Return may by empty if A is contained inside B.
 * @note The originals are undisturbed.
 */
static bspbrush_t *SubtractBrush (bspbrush_t *a, const bspbrush_t *b)
{
	/* a - b = out (list) */
	int i;
	bspbrush_t *front, *back, *out;
	bspbrush_t *in;

	in = a;
	out = NULL;
	for (i = 0; i < b->numsides && in; i++) {
		SplitBrush(in, b->sides[i].planenum, &front, &back);
		if (in != a)
			FreeBrush(in);
		if (front) {	/* add to list */
			front->next = out;
			out = front;
		}
		in = back;
	}
	if (in)
		FreeBrush(in);
	else {	/* didn't really intersect */
		FreeBrushList(out);
		return a;
	}
	return out;
}

/**
 * @return true if the two brushes definately do not intersect.
 * @note There will be false negatives for some non-axial combinations.
 */
static qboolean BrushesDisjoint (bspbrush_t *a, bspbrush_t *b)
{
	int i, j;

	/* check bounding boxes */
	for (i = 0; i < 3; i++)
		if (a->mins[i] >= b->maxs[i] || a->maxs[i] <= b->mins[i])
			return qtrue;	/* bounding boxes don't overlap */

	/* check for opposing planes */
	for (i = 0; i < a->numsides; i++) {
		for (j = 0; j < b->numsides; j++) {
			if (a->sides[i].planenum == (b->sides[j].planenum ^ 1))
				return qtrue;	/* opposite planes, so not touching */
		}
	}

	return qfalse;	/* might intersect */
}

static uint16_t minplanenums[2];
static uint16_t maxplanenums[2];

/**
 * @brief Any planes shared with the box edge will be set to no texinfo
 * @note not thread safe
 */
static bspbrush_t *ClipBrushToBox (bspbrush_t *brush, vec3_t clipmins, vec3_t clipmaxs)
{
	int i, j;
	bspbrush_t *front, *back;

	for (j = 0; j < 2; j++) {
		if (brush->maxs[j] > clipmaxs[j]) {
			SplitBrush(brush, maxplanenums[j], &front, &back);
			FreeBrush(brush);
			if (front)
				FreeBrush(front);
			brush = back;
			if (!brush)
				return NULL;
		}
		if (brush->mins[j] < clipmins[j]) {
			SplitBrush(brush, minplanenums[j], &front, &back);
			FreeBrush(brush);
			if (back)
				FreeBrush(back);
			brush = front;
			if (!brush)
				return NULL;
		}
	}

	/* remove any colinear faces */
	for (i = 0; i < brush->numsides; i++) {
		side_t *side = &brush->sides[i];
		const int p = side->planenum & ~1;
		if (p == maxplanenums[0] || p == maxplanenums[1]
			|| p == minplanenums[0] || p == minplanenums[1]) {
			side->texinfo = TEXINFO_NODE;
			side->visible = qfalse;
		}
	}
	return brush;
}

/**
 * @brief checks if the level# matches the contentsmask.
 * The level# is mapped to the appropriate levelflags.
 * @param[in] contents The contentsmask (of the brush, surface, etc.) to check
 * @param[in] level -1 for skipping the levelflag check
 * @return boolean value
 */
static qboolean IsInLevel (const int contents, const int level)
{
	/* special levels */
	switch (level) {
	case LEVEL_LIGHTCLIP:
		if (contents & CONTENTS_LIGHTCLIP)
			return qtrue;
		else
			return qfalse;
	case LEVEL_WEAPONCLIP:
		if (contents & CONTENTS_WEAPONCLIP)
			return qtrue;
		else
			return qfalse;
	case LEVEL_ACTORCLIP:
		if (contents & CONTENTS_ACTORCLIP)
			return qtrue;
		else
			return qfalse;
	}

	/* If the brush is any kind of clip, we are not looking for it after here. */
	if (contents & MASK_CLIP)
		return qfalse;

	/* standard levels */
	if (level == -1)
		return qtrue;
	else if (level) {
		if (((contents >> 8) & 0xFF) == level)
			return qtrue;
		else
			return qfalse;
	} else {
		if (contents & 0xFF00)
			return qfalse;
		else
			return qtrue;
	}
}

static bspbrush_t *AddBrushListToTail (bspbrush_t *list, bspbrush_t *tail)
{
	bspbrush_t *walk, *next;

	/* add to end of list */
	for (walk = list; walk; walk = next) {
		next = walk->next;
		walk->next = NULL;
		tail->next = walk;
		tail = walk;
	}

	return tail;
}

/**
 * @brief Builds a new list that doesn't hold the given brush
 * @param[in] list The brush list to copy
 * @param[in] skip The brush to skip
 * @return a @c bspbrush_t that is the old list without the skip entry
 */
static bspbrush_t *CullList (bspbrush_t *list, bspbrush_t *skip)
{
	bspbrush_t *newlist;
	bspbrush_t *next;

	newlist = NULL;

	for (; list; list = next) {
		next = list->next;
		if (list == skip) {
			FreeBrush(list);
			continue;
		}
		list->next = newlist;
		newlist = list;
	}
	return newlist;
}

/**
 * @brief Returns true if b1 is allowed to bite b2
 */
static inline qboolean BrushGE (bspbrush_t *b1, bspbrush_t *b2)
{
	/* detail brushes never bite structural brushes */
	if ((b1->original->contentFlags & CONTENTS_DETAIL)
		&& !(b2->original->contentFlags & CONTENTS_DETAIL))
		return qfalse;
	if (b1->original->contentFlags & CONTENTS_SOLID)
		return qtrue;
	return qfalse;
}

/**
 * @brief sets mins and maxs to the smallest sizes that can contain all brushes from startbrush
 * to endbrush that are in a given level.
 * @param[in] startbrush the index of the first brush to check.
 * @param[in] endbrush the index after the last brush to check.
 * @param[in] level the level that we are searching for brushes in. -1 for skipping the levelflag check.
 * @param[in] clipmins the absolute lowest boundary to allow for brushes.
 * @param[in] clipmaxs the absolute highest boundary to allow for brushes.
 * @param[out] mins the lowest boundary for all accepted brushes within the clipped bounds.
 * @param[out] maxs the highest boundary for all accepted brushes within the clipped bounds.
 * @sa ProcessSubModel
 * @sa IsInLevel
 */
int MapBrushesBounds (const int startbrush, const int endbrush, const int level, const vec3_t clipmins, const vec3_t clipmaxs, vec3_t mins, vec3_t maxs)
{
	int i, j, num;

	ClearBounds(mins, maxs);
	num = 0;

	for (i = startbrush; i < endbrush; i++) {
		const mapbrush_t *b = &mapbrushes[i];

		/* don't use finished brushes again */
		if (b->finished)
			continue;

		if (!IsInLevel(b->contentFlags, level))
			continue;

		/* check the bounds */
		for (j = 0; j < 3; j++)
			if (b->mins[j] < clipmins[j]
			 || b->maxs[j] > clipmaxs[j])
			break;
		if (j != 3)
			continue;

		num++;
		AddPointToBounds(b->mins, mins, maxs);
		AddPointToBounds(b->maxs, mins, maxs);
	}

	return num;
}

bspbrush_t *MakeBspBrushList (int startbrush, int endbrush, int level, vec3_t clipmins, vec3_t clipmaxs)
{
	bspbrush_t *brushlist, *newbrush;
	int i, j, vis;
	int c_faces, c_brushes, numsides;

	Verb_Printf(VERB_DUMP, "MakeBspBrushList: bounds (%f %f %f) (%f %f %f)\n",
		clipmins[0], clipmins[1], clipmins[2], clipmaxs[0], clipmaxs[1], clipmaxs[2]);

	for (i = 0; i < 2; i++) {
		float dist;
		vec3_t normal = {0, 0, 0};
		normal[i] = 1;
		dist = clipmaxs[i];
		maxplanenums[i] = FindOrCreateFloatPlane(normal, dist);
		dist = clipmins[i];
		minplanenums[i] = FindOrCreateFloatPlane(normal, dist);
	}

	brushlist = NULL;
	c_faces = 0;
	c_brushes = 0;

	for (i = startbrush; i < endbrush; i++) {
		mapbrush_t *mb = &mapbrushes[i];

		if (!IsInLevel(mb->contentFlags, level)){
			Verb_Printf(VERB_DUMP, "Rejected brush %i: wrong level.\n", i);
			continue;
		}

		if (mb->finished){
			Verb_Printf(VERB_DUMP, "Rejected brush %i: already finished.\n", i);
			continue;
		}

		numsides = mb->numsides;
		if (!numsides) {
			Verb_Printf(VERB_DUMP, "Rejected brush %i: no sides.\n", i);
			continue;
		}

		/* make sure the brush has at least one face showing */
		vis = 0;
		for (j = 0; j < numsides; j++)
			if (mb->original_sides[j].visible && mb->original_sides[j].winding)
				vis++;

		/* if the brush is outside the clip area, skip it */
		for (j = 0; j < 3; j++)
			if (mb->mins[j] < clipmins[j] || mb->maxs[j] > clipmaxs[j])
				break;
		if (j != 3)
			continue;

		mb->finished = qtrue;

		/* make a copy of the brush */
		newbrush = AllocBrush(mb->numsides);
		newbrush->original = mb;
		newbrush->numsides = mb->numsides;
		memcpy(newbrush->sides, mb->original_sides, numsides * sizeof(side_t));
		for (j = 0; j < numsides; j++) {
			side_t *side = &newbrush->sides[j];
			if (side->winding)
				side->winding = CopyWinding(side->winding);
			if (side->surfaceFlags & SURF_HINT)
				side->visible = qtrue; /* hints are always visible */
		}
		VectorCopy(mb->mins, newbrush->mins);
		VectorCopy(mb->maxs, newbrush->maxs);

		/* carve off anything outside the clip box */
		newbrush = ClipBrushToBox(newbrush, clipmins, clipmaxs);
		if (!newbrush) {
			Verb_Printf(VERB_DUMP, "Rejected brush %i: cannot clip to box.\n", i);
			continue;
		}

		c_faces += vis;
		c_brushes++;

		newbrush->next = brushlist;
		brushlist = newbrush;
		Verb_Printf(VERB_DUMP, "Added brush %i.\n", i);
	}

	return brushlist;
}

/**
 * @brief Carves any intersecting solid brushes into the minimum number of non-intersecting brushes.
 */
bspbrush_t *ChopBrushes (bspbrush_t *head)
{
	bspbrush_t *b1, *b2, *next;
	bspbrush_t *tail;
	bspbrush_t *keep;
	bspbrush_t *sub, *sub2;
	int c1, c2;
	const int originalBrushes = CountBrushList(head);
	int keepBrushes;

	Verb_Printf(VERB_EXTRA, "---- ChopBrushes ----\n");

	keep = NULL;

newlist:
	/* find tail */
	if (!head)
		return NULL;

	for (tail = head; tail->next; tail = tail->next);

	for (b1 = head; b1; b1 = next) {
		next = b1->next;
		for (b2 = b1->next; b2; b2 = b2->next) {
			if (BrushesDisjoint(b1, b2))
				continue;

			sub = sub2 = NULL;
			c1 = c2 = 999999;

			if (BrushGE(b2, b1)) {
				sub = SubtractBrush(b1, b2);
				if (sub == b1)
					continue;		/* didn't really intersect */
				if (!sub) {	/* b1 is swallowed by b2 */
					head = CullList(b1, b1);
					goto newlist;
				}
				c1 = CountBrushList(sub);
			}

			if (BrushGE(b1, b2)) {
				sub2 = SubtractBrush(b2, b1);
				if (sub2 == b2)
					continue;		/* didn't really intersect */
				if (!sub2) {	/* b2 is swallowed by b1 */
					FreeBrushList(sub);
					head = CullList(b1, b2);
					goto newlist;
				}
				c2 = CountBrushList(sub2);
			}

			if (!sub && !sub2)
				continue;		/* neither one can bite */

			/* only accept if it didn't fragment */
			/* (commenting this out allows full fragmentation) */
			if (c1 > 1 && c2 > 1) {
				if (sub2)
					FreeBrushList(sub2);
				if (sub)
					FreeBrushList(sub);
				continue;
			}

			if (c1 < c2) {
				if (sub2)
					FreeBrushList(sub2);
				tail = AddBrushListToTail(sub, tail);
				head = CullList(b1, b1);
				goto newlist;
			} else {
				if (sub)
					FreeBrushList(sub);
				tail = AddBrushListToTail(sub2, tail);
				head = CullList(b1, b2);
				goto newlist;
			}
		}

		/* b1 is no longer intersecting anything, so keep it */
		if (!b2) {
			b1->next = keep;
			keep = b1;
		}
	}

	keepBrushes = CountBrushList(keep);
	if (keepBrushes != originalBrushes) {
		Verb_Printf(VERB_EXTRA, "original brushes: %i\n", originalBrushes);
		Verb_Printf(VERB_EXTRA, "output brushes: %i\n", keepBrushes);
	}
	return keep;
}
