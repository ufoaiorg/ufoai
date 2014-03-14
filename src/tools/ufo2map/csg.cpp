/**
 * @file
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
static bspbrush_t* SubtractBrush (bspbrush_t* a, const bspbrush_t* b)
{
	/* a - b = out (list) */
	bspbrush_t* front, *back, *out;
	bspbrush_t* in;

	in = a;
	out = nullptr;
	for (int i = 0; i < b->numsides && in; i++) {
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
static bool BrushesDisjoint (bspbrush_t* a, bspbrush_t* b)
{
	int i;

	/* check bounding boxes */
	for (i = 0; i < 3; i++)
		if (a->brBox.mins[i] >= b->brBox.maxs[i] || a->brBox.maxs[i] <= b->brBox.mins[i])
			return true;	/* bounding boxes don't overlap */

	/* check for opposing planes */
	for (i = 0; i < a->numsides; i++) {
		for (int j = 0; j < b->numsides; j++) {
			if (a->sides[i].planenum == (b->sides[j].planenum ^ 1))
				return true;	/* opposite planes, so not touching */
		}
	}

	return false;	/* might intersect */
}

static uint16_t minplanenums[2];
static uint16_t maxplanenums[2];

/**
 * @brief Any planes shared with the box edge will be set to no texinfo
 * @note not thread safe
 */
static bspbrush_t* ClipBrushToBox (bspbrush_t* brush, const vec3_t clipmins, const vec3_t clipmaxs)
{
	bspbrush_t* front, *back;

	for (int j = 0; j < 2; j++) {
		if (brush->brBox.maxs[j] > clipmaxs[j]) {
			SplitBrush(brush, maxplanenums[j], &front, &back);
			FreeBrush(brush);
			if (front)
				FreeBrush(front);
			brush = back;
			if (!brush)
				return nullptr;
		}
		if (brush->brBox.mins[j] < clipmins[j]) {
			SplitBrush(brush, minplanenums[j], &front, &back);
			FreeBrush(brush);
			if (back)
				FreeBrush(back);
			brush = front;
			if (!brush)
				return nullptr;
		}
	}

	/* remove any colinear faces */
	for (int i = 0; i < brush->numsides; i++) {
		side_t* side = &brush->sides[i];
		const int p = side->planenum & ~1;
		if (p == maxplanenums[0] || p == maxplanenums[1]
			|| p == minplanenums[0] || p == minplanenums[1]) {
			side->texinfo = TEXINFO_NODE;
			side->visible = false;
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
static bool IsInLevel (const int contents, const int level)
{
	/* special levels */
	switch (level) {
	case LEVEL_LIGHTCLIP:
		if (contents & CONTENTS_LIGHTCLIP)
			return true;
		else
			return false;
	case LEVEL_WEAPONCLIP:
		if (contents & CONTENTS_WEAPONCLIP)
			return true;
		else
			return false;
	case LEVEL_ACTORCLIP:
		if (contents & CONTENTS_ACTORCLIP)
			return true;
		else
			return false;
	}

	/* If the brush is any kind of clip, we are not looking for it after here. */
	if (contents & MASK_CLIP)
		return false;

	/* standard levels */
	if (level == -1)
		return true;
	else if (level) {
		if (((contents >> 8) & 0xFF) == level)
			return true;
		else
			return false;
	} else {
		if (contents & 0xFF00)
			return false;
		else
			return true;
	}
}

static bspbrush_t* AddBrushListToTail (bspbrush_t* list, bspbrush_t* tail)
{
	bspbrush_t* walk, *next;

	/* add to end of list */
	for (walk = list; walk; walk = next) {
		next = walk->next;
		walk->next = nullptr;
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
static bspbrush_t* CullList (bspbrush_t* list, bspbrush_t* skip)
{
	bspbrush_t* newlist;
	bspbrush_t* next;

	newlist = nullptr;

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
static inline bool BrushGE (bspbrush_t* b1, bspbrush_t* b2)
{
	/* detail brushes never bite structural brushes */
	if ((b1->original->contentFlags & CONTENTS_DETAIL)
		&& !(b2->original->contentFlags & CONTENTS_DETAIL))
		return false;
	if (b1->original->contentFlags & CONTENTS_SOLID)
		return true;
	return false;
}

/**
 * @brief sets mins and maxs to the smallest sizes that can contain all brushes from startbrush
 * to endbrush that are in a given level.
 * @param[in] startbrush the index of the first brush to check.
 * @param[in] endbrush the index after the last brush to check.
 * @param[in] level the level that we are searching for brushes in. -1 for skipping the levelflag check.
 * @param[in] clipBox the absolute lowest and highest boundaries to allow for brushes.
 * @param[out] mins the lowest boundary for all accepted brushes within the clipped bounds.
 * @param[out] maxs the highest boundary for all accepted brushes within the clipped bounds.
 * @sa ProcessSubModel
 * @sa IsInLevel
 */
int MapBrushesBounds (const int startbrush, const int endbrush, const int level, const AABB& clipBox, vec3_t mins, vec3_t maxs)
{
	int i, num;

	ClearBounds(mins, maxs);
	num = 0;

	for (i = startbrush; i < endbrush; i++) {
		const mapbrush_t* b = &mapbrushes[i];

		/* don't use finished brushes again */
		if (b->finished)
			continue;

		if (!IsInLevel(b->contentFlags, level))
			continue;

		/* check the bounds */
		if (!clipBox.contains(b->mbBox))
			continue;

		num++;
		AddPointToBounds(b->mbBox.mins, mins, maxs);
		AddPointToBounds(b->mbBox.maxs, mins, maxs);
	}

	return num;
}

bspbrush_t* MakeBspBrushList (int startbrush, int endbrush, int level, const AABB& clip)
{
	bspbrush_t* brushlist, *newbrush;
	int i, j, vis;
	int numsides;

	Verb_Printf(VERB_DUMP, "MakeBspBrushList: bounds (%f %f %f) (%f %f %f)\n",
		clip.mins[0], clip.mins[1], clip.mins[2], clip.maxs[0], clip.maxs[1], clip.maxs[2]);

	for (i = 0; i < 2; i++) {
		float dist;
		vec3_t normal = {0, 0, 0};
		normal[i] = 1;
		dist = clip.maxs[i];
		maxplanenums[i] = FindOrCreateFloatPlane(normal, dist);
		dist = clip.mins[i];
		minplanenums[i] = FindOrCreateFloatPlane(normal, dist);
	}

	brushlist = nullptr;

	for (i = startbrush; i < endbrush; i++) {
		mapbrush_t* mb = &mapbrushes[i];

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
		if (!clip.contains(mb->mbBox))
			continue;

		mb->finished = true;

		/* make a copy of the brush */
		newbrush = AllocBrush(mb->numsides);
		newbrush->original = mb;
		newbrush->numsides = mb->numsides;
		memcpy(newbrush->sides, mb->original_sides, numsides * sizeof(side_t));
		for (j = 0; j < numsides; j++) {
			side_t* side = &newbrush->sides[j];
			if (side->winding)
				side->winding = CopyWinding(side->winding);
			if (side->surfaceFlags & SURF_HINT)
				side->visible = true; /* hints are always visible */
		}
		newbrush->brBox.set(mb->mbBox);

		/* carve off anything outside the clip box */
		newbrush = ClipBrushToBox(newbrush, clip.mins, clip.maxs);
		if (!newbrush) {
			Verb_Printf(VERB_DUMP, "Rejected brush %i: cannot clip to box.\n", i);
			continue;
		}

		newbrush->next = brushlist;
		brushlist = newbrush;
		Verb_Printf(VERB_DUMP, "Added brush %i.\n", i);
	}

	return brushlist;
}

/**
 * @brief Carves any intersecting solid brushes into the minimum number of non-intersecting brushes.
 */
bspbrush_t* ChopBrushes (bspbrush_t* head)
{
	bspbrush_t* b1, *b2, *next;
	bspbrush_t* tail;
	bspbrush_t* keep;
	bspbrush_t* sub, *sub2;
	int c1, c2;
	const int originalBrushes = CountBrushList(head);
	int keepBrushes;

	Verb_Printf(VERB_EXTRA, "---- ChopBrushes ----\n");

	keep = nullptr;

newlist:
	/* find tail */
	if (!head)
		return nullptr;

	for (tail = head; tail->next; tail = tail->next);

	for (b1 = head; b1; b1 = next) {
		next = b1->next;
		for (b2 = b1->next; b2; b2 = b2->next) {
			if (BrushesDisjoint(b1, b2))
				continue;

			sub = sub2 = nullptr;
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
