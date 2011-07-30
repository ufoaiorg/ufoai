/**
 * @file bspbrush.c
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
#include "map.h"

extern int c_nodes;
extern int c_nonvis;
static int c_active_brushes;


/**
 * @brief Sets the mins/maxs based on the windings
 */
static void BoundBrush (bspbrush_t *brush)
{
	int i, j;
	winding_t *w;

	ClearBounds(brush->mins, brush->maxs);
	for (i = 0; i < brush->numsides; i++) {
		w = brush->sides[i].winding;
		if (!w)
			continue;
		for (j = 0; j < w->numpoints; j++)
			AddPointToBounds(w->p[j], brush->mins, brush->maxs);
	}
}


/**
 * @brief makes basewindigs for sides and mins/maxs for the brush
 * @returns false if the brush doesn't enclose a valid volume
 */
static void CreateBrushWindings (bspbrush_t *brush)
{
	int i;

	for (i = 0; i < brush->numsides; i++) {
		side_t *side = &brush->sides[i];
		const plane_t *plane = &mapplanes[side->planenum];
		int j;

		/* evidence that winding_t represents a hessian normal plane */
		winding_t *w = BaseWindingForPlane(plane->normal, plane->dist);

		for (j = 0; j < brush->numsides && w; j++) {
			if (i == j)
				continue;
			/* back side clipaway */
			if (brush->sides[j].planenum == (side->planenum ^ 1))
				continue;
			if (brush->sides[j].bevel)
				continue;
			plane = &mapplanes[brush->sides[j].planenum ^ 1];
			ChopWindingInPlace(&w, plane->normal, plane->dist, 0); /*CLIP_EPSILON); */

			/* fix broken windings that would generate trifans */
			if (!FixWinding(w))
				Verb_Printf(VERB_EXTRA, "removed degenerated edge(s) from winding\n");
		}

		side->winding = w;
	}

	BoundBrush(brush);
}

/**
 * @brief Creates a new axial brush
 */
bspbrush_t *BrushFromBounds (vec3_t mins, vec3_t maxs)
{
	bspbrush_t *b;
	int i;
	vec3_t normal;
	vec_t dist;

	b = AllocBrush(6);
	b->numsides = 6;
	for (i = 0; i < 3; i++) {
		VectorClear(normal);
		normal[i] = 1;
		dist = maxs[i];
		b->sides[i].planenum = FindOrCreateFloatPlane(normal, dist);

		normal[i] = -1;
		dist = -mins[i];
		b->sides[3 + i].planenum = FindOrCreateFloatPlane(normal, dist);
	}

	CreateBrushWindings(b);

	return b;
}

/**
 * @brief Returns the volume of the given brush
 */
static vec_t BrushVolume (const bspbrush_t *brush)
{
	int i;
	const winding_t *w;
	vec3_t corner;
	vec_t volume;

	if (!brush)
		return 0;

	/* grab the first valid point as the corner */
	w = NULL;
	for (i = 0; i < brush->numsides; i++) {
		w = brush->sides[i].winding;
		if (w)
			break;
	}
	if (!w)
		return 0;
	VectorCopy(w->p[0], corner);

	/* make tetrahedrons to all other faces */
	volume = 0;
	for (; i < brush->numsides; i++) {
		w = brush->sides[i].winding;
		if (w) {
			const plane_t *plane = &mapplanes[brush->sides[i].planenum];
			const vec_t d = -(DotProduct(corner, plane->normal) - plane->dist);
			const vec_t area = WindingArea(w);
			volume += d * area;
		}
	}

	volume /= 3;
	return volume;
}

/**
 * @brief Returns the amount of brushes in the given brushlist
 */
int CountBrushList (bspbrush_t *brushes)
{
	int c;

	c = 0;
	for (; brushes; brushes = brushes->next)
		c++;
	return c;
}

/**
 * @sa AllocTree
 * @sa AllocNode
 */
bspbrush_t *AllocBrush (int numsides)
{
	if (threadstate.numthreads == 1)
		c_active_brushes++;

	return (bspbrush_t *)Mem_Alloc(offsetof(bspbrush_t, sides[numsides]));
}

/**
 * @sa AllocBrush
 */
void FreeBrush (bspbrush_t *brushes)
{
	int i;

	for (i = 0; i < brushes->numsides; i++)
		if (brushes->sides[i].winding)
			FreeWinding(brushes->sides[i].winding);
	Mem_Free(brushes);
	if (threadstate.numthreads == 1)
		c_active_brushes--;
}

/**
 * @sa AllocBrush
 * @sa CountBrushList
 */
void FreeBrushList (bspbrush_t *brushes)
{
	bspbrush_t *next;

	for (; brushes; brushes = next) {
		next = brushes->next;
		FreeBrush(brushes);
	}
}

/**
 * @brief Duplicates the brush, the sides, and the windings
 * @sa AllocBrush
 */
bspbrush_t *CopyBrush (const bspbrush_t *brush)
{
	bspbrush_t *newbrush;
	int i;
	size_t size = offsetof(bspbrush_t, sides[brush->numsides]);

	newbrush = AllocBrush(brush->numsides);
	memcpy(newbrush, brush, size);

	for (i = 0; i < brush->numsides; i++) {
		const side_t *side = &brush->sides[i];
		if (side->winding)
			newbrush->sides[i].winding = CopyWinding(side->winding);
	}

	return newbrush;
}


static int TestBrushToPlanenum (bspbrush_t *brush, uint16_t planenum,
			int *numsplits, qboolean *hintsplit, int *epsilonbrush)
{
	int i, s;
	plane_t *plane;
	dBspPlane_t plane2;
	vec_t d_front, d_back;
	int front, back;

	*numsplits = 0;
	*hintsplit = qfalse;

	/* if the brush actually uses the planenum,
	 * we can tell the side for sure */
	for (i = 0; i < brush->numsides; i++) {
		const uint16_t num = brush->sides[i].planenum;
		if (num == planenum)
			return (PSIDE_BACK | PSIDE_FACING);
		if (num == (planenum ^ 1))
			return (PSIDE_FRONT | PSIDE_FACING);
	}

	/* box on plane side */
	plane = &mapplanes[planenum];

	/* Convert to cBspPlane */
	VectorCopy(plane->normal, plane2.normal);
	plane2.dist = plane->dist;
	plane2.type = plane->type;
	s = TR_BoxOnPlaneSide(brush->mins, brush->maxs, &plane2);

	if (s != PSIDE_BOTH)
		return s;

	/* if both sides, count the visible faces split */
	d_front = d_back = 0;

	for (i = 0; i < brush->numsides; i++) {
		const side_t *side = &brush->sides[i];
		const winding_t *w;
		int j;
		if (side->texinfo == TEXINFO_NODE)
			continue;		/* on node, don't worry about splits */
		if (!side->visible)
			continue;		/* we don't care about non-visible */
		w = side->winding;
		if (!w)
			continue;
		front = back = 0;
		for (j = 0; j < w->numpoints; j++) {
			const vec_t d = DotProduct(w->p[j], plane->normal) - plane->dist;
			if (d > d_front)
				d_front = d;
			if (d < d_back)
				d_back = d;

			if (d > 0.1) /* PLANESIDE_EPSILON) */
				front = 1;
			else if (d < -0.1) /* PLANESIDE_EPSILON) */
				back = 1;
		}
		if (front && back) {
			(*numsplits)++;
			if (side->surfaceFlags & SURF_HINT)
				*hintsplit = qtrue;
		}
	}

	if ((d_front > 0.0 && d_front < 1.0) || (d_back < 0.0 && d_back > -1.0))
		(*epsilonbrush)++;

	return s;
}

/**
 * @brief Collects the contentsflags of the brushes in the given list
 */
uint32_t BrushListCalcContents (bspbrush_t *brushlist)
{
	bspbrush_t *b;
	uint32_t contentFlags = 0;

	for (b = brushlist; b; b = b->next) {
		/* if the brush is solid and all of its sides are on nodes,
		 * it eats everything */
		if ((b->original->contentFlags & CONTENTS_SOLID) && !(b->original->contentFlags & CONTENTS_PASSABLE)) {
			int i;
			for (i = 0; i < b->numsides; i++)
				if (b->sides[i].texinfo != TEXINFO_NODE)
					break;
			if (i == b->numsides) {
				contentFlags = CONTENTS_SOLID;
				break;
			}
		}
		contentFlags |= b->original->contentFlags;
	}

	return contentFlags;
}

/**
 * @brief Checks on which side a of plane the brush is
 * @todo Or vice versa?
 */
static int BrushMostlyOnSide (const bspbrush_t *brush, const plane_t *plane)
{
	int i, j, side;
	vec_t max;

	max = 0;
	side = PSIDE_FRONT;
	for (i = 0; i < brush->numsides; i++) {
		const winding_t *w = brush->sides[i].winding;
		if (!w)
			continue;
		for (j = 0; j < w->numpoints; j++) {
			const vec_t d = DotProduct(w->p[j], plane->normal) - plane->dist;
			if (d > max) {
				max = d;
				side = PSIDE_FRONT;
			}
			if (-d > max) {
				max = -d;
				side = PSIDE_BACK;
			}
		}
	}
	return side;
}

#define TESTING_MOCK_SPLIT 1
#if TESTING_MOCK_SPLIT
/**
 * @brief Checks if the plane splits the brush
 */
static qboolean DoesPlaneSplitBrush (const bspbrush_t *brush, int planenum)
{
	int i, j;
	winding_t *w;
	plane_t *plane  = &mapplanes[planenum];
	int front_cnt = 0, back_cnt = 0;

	/* check all points */
	for (i = 0; i < brush->numsides; i++) {
		w = brush->sides[i].winding;
		if (!w)
			continue;
		for (j = 0; j < w->numpoints; j++) {
			const float d = DotProduct(w->p[j], plane->normal) - plane->dist;
			if (d > 0.1f) /* PLANESIDE_EPSILON) */
				front_cnt++;
			if (d < -0.1f) /* PLANESIDE_EPSILON) */
				back_cnt++;
		}
	}

	/* if brush vertices are both in front and back of given plane, in splits brush */
	return front_cnt && back_cnt ;
}
#endif

#if TESTING_MOCK_SPLIT
/* DoesPlaneSplitBrush does not yet work for all maps, namely city_train.map
 * Until we know why, let's use the old stuff. */
static qboolean CheckPlaneAgainstVolume (int pnum, const bspbrush_t *volume)
{
	qboolean good;

	good = DoesPlaneSplitBrush(volume, pnum);

	return good;
}

#else
static qboolean CheckPlaneAgainstVolume (uint16_t pnum, const bspbrush_t *volume)
{
		bspbrush_t *front, *back;
		qboolean good;

		SplitBrush(volume, pnum, &front, &back);

		good = (front && back);

		if (front)
				FreeBrush(front);
		if (back)
				FreeBrush(back);

		return good;
}
#endif
/**
 * @brief Using a heuristic, choses one of the sides out of the brushlist
 * to partition the brushes with.
 * @return NULL if there are no valid planes to split with..
 */
side_t *SelectSplitSide (bspbrush_t *brushes, bspbrush_t *volume)
{
	int value, bestvalue;
	bspbrush_t *brush, *test;
	side_t *bestside;
	int i, j, pass, numpasses;
	int front, back, both, facing, splits;
	int bsplits, epsilonbrush;
	qboolean hintsplit;

	if (!volume)
		return NULL; /* can't split empty brush */

	bestside = NULL;
	bestvalue = -99999;

	/**
	 * the search order goes: visible-structural, visible-detail,
	 * nonvisible-structural, nonvisible-detail.
	 * If any valid plane is available in a pass, no further
	 * passes will be tried.
	 */
	numpasses = 4;
	for (pass = 0; pass < numpasses; pass++) {
		for (brush = brushes; brush; brush = brush->next) {
			if ((pass & 1) && !(brush->original->contentFlags & CONTENTS_DETAIL))
				continue;
			if (!(pass & 1) && (brush->original->contentFlags & CONTENTS_DETAIL))
				continue;
			for (i = 0; i < brush->numsides; i++) {
				uint16_t pnum;
				side_t *side = &brush->sides[i];
				if (side->bevel)
					continue;	/* never use a bevel as a spliter */
				if (!side->winding)
					continue;	/* nothing visible, so it can't split */
				if (side->texinfo == TEXINFO_NODE)
					continue;	/* already a node splitter */
				if (side->tested)
					continue;	/* we already have metrics for this plane */
				if (side->surfaceFlags & SURF_SKIP)
					continue;	/* skip surfaces are never chosen */
				if (side->visible ^ (pass < 2))
					continue;	/* only check visible faces on first pass */

				pnum = side->planenum;
				pnum &= ~1;	/* always use positive facing plane */

				if (!CheckPlaneAgainstVolume(pnum, volume))
					continue;	/* would produce a tiny volume */

				front = 0;
				back = 0;
				both = 0;
				facing = 0;
				splits = 0;
				epsilonbrush = 0;
				hintsplit = qfalse;

				for (test = brushes; test; test = test->next) {
					const int s = TestBrushToPlanenum(test, pnum, &bsplits, &hintsplit, &epsilonbrush);

					splits += bsplits;
					if (bsplits && (s & PSIDE_FACING))
						Sys_Error("PSIDE_FACING with splits");

					test->testside = s;
					/* if the brush shares this face, don't bother
					 * testing that facenum as a splitter again */
					if (s & PSIDE_FACING) {
						facing++;
						for (j = 0; j < test->numsides; j++) {
							if ((test->sides[j].planenum &~ 1) == pnum)
								test->sides[j].tested = qtrue;
						}
					}
					if (s & PSIDE_FRONT)
						front++;
					if (s & PSIDE_BACK)
						back++;
					if (s == PSIDE_BOTH)
						both++;
				}

				/* give a value estimate for using this plane */

				value = 5 * facing - 5 * splits - abs(front - back);
				if (AXIAL(&mapplanes[pnum]))
					value += 5;		/* axial is better */
				value -= epsilonbrush * 1000;	/* avoid! */

				/* never split a hint side except with another hint */
				if (hintsplit && !(side->surfaceFlags & SURF_HINT))
					value = -9999999;

				/* save off the side test so we don't need
				 * to recalculate it when we actually seperate
				 * the brushes */
				if (value > bestvalue) {
					bestvalue = value;
					bestside = side;
					for (test = brushes; test; test = test->next)
						test->side = test->testside;
				}
			}
		}

		/* if we found a good plane, don't bother trying any other passes */
		if (bestside) {
			if (pass > 1) {
				if (threadstate.numthreads == 1)
					c_nonvis++;
			}
			break;
		}
	}

	/* clear all the tested flags we set */
	for (brush = brushes; brush; brush = brush->next) {
		for (i = 0; i < brush->numsides; i++)
			brush->sides[i].tested = qfalse;
	}

	return bestside;
}

/**
 * @brief Generates two new brushes, leaving the original unchanged
 */
void SplitBrush (const bspbrush_t *brush, uint16_t planenum, bspbrush_t **front, bspbrush_t **back)
{
	bspbrush_t *b[2];
	int i, j;
	winding_t *w, *cw[2], *midwinding;
	plane_t *plane, *plane2;
	float d_front, d_back;

	*front = *back = NULL;
	plane = &mapplanes[planenum];

	/* check all points */
	d_front = d_back = 0;
	for (i = 0; i < brush->numsides; i++) {
		w = brush->sides[i].winding;
		if (!w)
			continue;
		for (j = 0; j < w->numpoints; j++) {
			const float d = DotProduct(w->p[j], plane->normal) - plane->dist;
			if (d > 0 && d > d_front)
				d_front = d;
			else if (d < 0 && d < d_back)
				d_back = d;
		}
	}
	if (d_front < 0.1) { /* PLANESIDE_EPSILON) */
		/* only on back */
		*back = CopyBrush(brush);
		return;
	}
	if (d_back > -0.1) { /* PLANESIDE_EPSILON) */
		/* only on front */
		*front = CopyBrush(brush);
		return;
	}

	/* create a new winding from the split plane */
	w = BaseWindingForPlane(plane->normal, plane->dist);
	for (i = 0; i < brush->numsides && w; i++) {
		plane2 = &mapplanes[brush->sides[i].planenum ^ 1];
		ChopWindingInPlace(&w, plane2->normal, plane2->dist, 0); /* PLANESIDE_EPSILON); */
	}

	/* the brush isn't really split */
	if (!w || WindingIsTiny(w)) {
		const int side = BrushMostlyOnSide(brush, plane);
		if (side == PSIDE_FRONT)
			*front = CopyBrush(brush);
		else if (side == PSIDE_BACK)
			*back = CopyBrush(brush);
		return;
	}

	if (WindingIsHuge(w)) {
		/** @todo Print brush and entnum either of the brush that was splitted
		 * or the plane that was used as splitplane */
		Com_Printf("WARNING: Large winding\n");
	}

	midwinding = w;

	/* split it for real */
	for (i = 0; i < 2; i++) {
		b[i] = AllocBrush(brush->numsides + 1);
		b[i]->original = brush->original;
	}

	/* split all the current windings */
	for (i = 0; i < brush->numsides; i++) {
		const side_t *s = &brush->sides[i];
		w = s->winding;
		if (!w)
			continue;
		ClipWindingEpsilon(w, plane->normal, plane->dist,
			0 /*PLANESIDE_EPSILON*/, &cw[0], &cw[1]);
		for (j = 0; j < 2; j++) {
			side_t *cs;

			if (!cw[j])
				continue;

			cs = &b[j]->sides[b[j]->numsides];
			b[j]->numsides++;
			*cs = *s;

			cs->winding = cw[j];
			cs->tested = qfalse;
		}
	}

	/* see if we have valid polygons on both sides */
	for (i = 0; i < 2; i++) {
		BoundBrush(b[i]);
		for (j = 0; j < 3; j++) {
			if (b[i]->mins[j] < -MAX_WORLD_WIDTH || b[i]->maxs[j] > MAX_WORLD_WIDTH) {
				/** @todo Print brush and entnum either of the brush that was split
				 * or the plane that was used as splitplane */
				Verb_Printf(VERB_EXTRA, "bogus brush after clip\n");
				break;
			}
		}

		if (b[i]->numsides < 3 || j < 3) {
			FreeBrush(b[i]);
			b[i] = NULL;
		}
	}

	if (!(b[0] && b[1])) {
		/** @todo Print brush and entnum either of the brush that was splitted
		 * or the plane that was used as splitplane */
		if (!b[0] && !b[1])
			Verb_Printf(VERB_EXTRA, "split removed brush\n");
		else
			Verb_Printf(VERB_EXTRA, "split not on both sides\n");
		if (b[0]) {
			FreeBrush(b[0]);
			*front = CopyBrush(brush);
		}
		if (b[1]) {
			FreeBrush(b[1]);
			*back = CopyBrush(brush);
		}
		return;
	}

	/* add the midwinding to both sides */
	for (i = 0; i < 2; i++) {
		side_t *cs = &b[i]->sides[b[i]->numsides];
		b[i]->numsides++;

		cs->planenum = planenum ^ i ^ 1;
		cs->texinfo = TEXINFO_NODE;
		cs->visible = qfalse;
		cs->tested = qfalse;
		if (i == 0)
			cs->winding = CopyWinding(midwinding);
		else
			cs->winding = midwinding;
	}

	for (i = 0; i < 2; i++) {
		const vec_t v1 = BrushVolume(b[i]);
		if (v1 < 1.0) {
			FreeBrush(b[i]);
			b[i] = NULL;
			/** @todo Print brush and entnum either of the brush that was splitted
			 * or the plane that was used as splitplane */
			Verb_Printf(VERB_EXTRA, "tiny volume after clip\n");
		}
	}

	*front = b[0];
	*back = b[1];
}

void SplitBrushList (bspbrush_t *brushes, uint16_t planenum, bspbrush_t **front, bspbrush_t **back)
{
	bspbrush_t *brush;

	*front = *back = NULL;

	for (brush = brushes; brush; brush = brush->next) {
		const int sides = brush->side;
		bspbrush_t *newbrush;

		if (sides == PSIDE_BOTH) {	/* split into two brushes */
			bspbrush_t *newbrush2;
			SplitBrush(brush, planenum, &newbrush, &newbrush2);
			if (newbrush) {
				newbrush->next = *front;
				Verb_Printf(VERB_DUMP, "SplitBrushList: Adding brush %i to front list.\n", newbrush->original->brushnum);
				*front = newbrush;
			}
			if (newbrush2) {
				newbrush2->next = *back;
				Verb_Printf(VERB_DUMP, "SplitBrushList: Adding brush %i to back list.\n", newbrush2->original->brushnum);
				*back = newbrush2;
			}
			continue;
		}

		newbrush = CopyBrush(brush);

		/* if the planenum is actually a part of the brush
		 * find the plane and flag it as used so it won't be tried
		 * as a splitter again */
		if (sides & PSIDE_FACING) {
			int i;
			for (i = 0; i < newbrush->numsides; i++) {
				side_t *side = newbrush->sides + i;
				if ((side->planenum & ~1) == planenum)
					side->texinfo = TEXINFO_NODE;
			}
		}

		if (sides & PSIDE_FRONT) {
			newbrush->next = *front;
			*front = newbrush;
			Verb_Printf(VERB_DUMP, "SplitBrushList: Adding brush %i to front list.\n", newbrush->original->brushnum);
			continue;
		}
		if (sides & PSIDE_BACK) {
			newbrush->next = *back;
			Verb_Printf(VERB_DUMP, "SplitBrushList: Adding brush %i to back list.\n", newbrush->original->brushnum);
			*back = newbrush;
			continue;
		}
		Verb_Printf(VERB_DUMP, "SplitBrushList: Brush %i fell off the map.\n", newbrush->original->brushnum);
	}
}


/**
 * @brief Counts the faces and calculate the aabb
 */
void BrushlistCalcStats (bspbrush_t *brushlist, vec3_t mins, vec3_t maxs)
{
	bspbrush_t *b;
	int c_faces = 0, c_nonvisfaces = 0, c_brushes = 0;

	for (b = brushlist; b; b = b->next) {
		int i;
		vec_t volume;

		c_brushes++;

		volume = BrushVolume(b);
		if (volume < config.microvolume) {
			Com_Printf("\nWARNING: entity %i, brush %i: microbrush, volume %.3g\n",
				b->original->entitynum, b->original->brushnum, volume);
		}

		for (i = 0; i < b->numsides; i++) {
			side_t *side = &b->sides[i];
			if (side->bevel)
				continue;
			if (!side->winding)
				continue;
			if (side->texinfo == TEXINFO_NODE)
				continue;
			if (side->visible)
				c_faces++;
			else
				c_nonvisfaces++;
		}

		AddPointToBounds(b->mins, mins, maxs);
		AddPointToBounds(b->maxs, mins, maxs);
	}

	Verb_Printf(VERB_EXTRA, "%5i brushes\n", c_brushes);
	Verb_Printf(VERB_EXTRA, "%5i visible faces\n", c_faces);
	Verb_Printf(VERB_EXTRA, "%5i nonvisible faces\n", c_nonvisfaces);
}
