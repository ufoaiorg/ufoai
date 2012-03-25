/**
 * @file portals.c
 * @brief imagine you have a series of rooms connected by doorways. Each doorway
 * has a portal in it. If two portals can see eachother, the rooms become linked
 * in the pvs
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


static int c_active_portals = 0;
static int c_peak_portals = 0;
static int c_tinyportals = 0;

/**
 * @sa FreePortal
 */
static portal_t *AllocPortal (void)
{
	if (threadstate.numthreads == 1)
		c_active_portals++;
	if (c_active_portals > c_peak_portals)
		c_peak_portals = c_active_portals;

	return Mem_AllocType(portal_t);
}

/**
 * @sa AllocPortal
 */
void FreePortal (portal_t *p)
{
	if (p->winding)
		FreeWinding(p->winding);
	if (threadstate.numthreads == 1)
		c_active_portals--;
	Mem_Free(p);
}

/**
 * @brief Returns the single content bit of the strongest visible content present
 */
uint32_t VisibleContents (uint32_t contentFlags)
{
	uint32_t i;

	for (i = 1; i <= LAST_VISIBLE_CONTENTS; i <<= 1)
		if (contentFlags & i)
			return i;

	return 0;
}

/**
 * @brief Links a portal into the given back and front nodes
 * @param[in,out] p The portal to link into the front and back nodes
 * @param[in,out] front The front node
 * @param[in,out] back The back node
 * @sa RemovePortalFromNode
 */
static void AddPortalToNodes (portal_t *p, node_t *front, node_t *back)
{
	if (p->nodes[0] || p->nodes[1])
		Sys_Error("AddPortalToNodes: already included");

	p->nodes[0] = front;
	p->next[0] = front->portals;
	front->portals = p;

	p->nodes[1] = back;
	p->next[1] = back->portals;
	back->portals = p;
}

/**
 * @brief Removes references to the given portal from the given node
 * @param[in,out] portal The portal to remove from the node
 * @param[in,out] l The node to remove the portal from
 * @sa AddPortalToNodes
 */
void RemovePortalFromNode (portal_t *portal, node_t *l)
{
	portal_t **pp;

	/* remove reference to the current portal */
	pp = &l->portals;
	while (1) {
		portal_t *t = *pp;
		if (!t)
			Sys_Error("RemovePortalFromNode: portal not in leaf");

		if (t == portal)
			break;

		if (t->nodes[0] == l)
			pp = &t->next[0];
		else if (t->nodes[1] == l)
			pp = &t->next[1];
		else
			Sys_Error("RemovePortalFromNode: portal not bounding leaf");
	}

	if (portal->nodes[0] == l) {
		*pp = portal->next[0];
		portal->nodes[0] = NULL;
	} else if (portal->nodes[1] == l) {
		*pp = portal->next[1];
		portal->nodes[1] = NULL;
	}
}

#define	SIDESPACE	8
/**
 * @brief The created portals will face the global outside_node
 */
static void MakeHeadnodePortals (tree_t *tree)
{
	vec3_t bounds[2];
	int i, j;
	portal_t *portals[6];
	plane_t bplanes[6];
	node_t *node;

	node = tree->headnode;

	/* pad with some space so there will never be null volume leafs */
	for (i = 0; i < 3; i++) {
		bounds[0][i] = tree->mins[i] - SIDESPACE;
		bounds[1][i] = tree->maxs[i] + SIDESPACE;
	}

	tree->outside_node.planenum = PLANENUM_LEAF;
	tree->outside_node.brushlist = NULL;
	tree->outside_node.portals = NULL;
	tree->outside_node.contentFlags = 0;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 2; j++) {
			const int n = j * 3 + i;
			portal_t *p = AllocPortal();
			plane_t *pl = &bplanes[n];

			OBJZERO(*pl);
			portals[n] = p;

			if (j) {
				pl->normal[i] = -1;
				pl->dist = -bounds[j][i];
			} else {
				pl->normal[i] = 1;
				pl->dist = bounds[j][i];
			}
			p->plane = *pl;
			p->winding = BaseWindingForPlane(pl->normal, pl->dist);
			AddPortalToNodes(p, node, &tree->outside_node);
		}

	/* clip the basewindings by all the other planes */
	for (i = 0; i < 6; i++) {
		for (j = 0; j < 6; j++) {
			if (j == i)
				continue;
			ChopWindingInPlace(&portals[i]->winding, bplanes[j].normal, bplanes[j].dist, ON_EPSILON);
		}
	}
}

#define	BASE_WINDING_EPSILON	0.001
#define	SPLIT_WINDING_EPSILON	0.001

static winding_t *BaseWindingForNode (node_t *node)
{
	node_t *n;
	winding_t *w;

	w = BaseWindingForPlane(mapplanes[node->planenum].normal
		, mapplanes[node->planenum].dist);

	/* clip by all the parents */
	for (n = node->parent; n && w;) {
		const plane_t *plane = &mapplanes[n->planenum];

		if (n->children[0] == node) {	/* take front */
			ChopWindingInPlace(&w, plane->normal, plane->dist, BASE_WINDING_EPSILON);
		} else {	/* take back */
			const vec_t dist = -plane->dist;
			vec3_t normal;
			VectorSubtract(vec3_origin, plane->normal, normal);
			ChopWindingInPlace(&w, normal, dist, BASE_WINDING_EPSILON);
		}
		node = n;
		n = n->parent;
	}

	return w;
}

/*============================================================ */

/**
 * @brief Create the new portal by taking the full plane winding for the cutting plane
 * and clipping it by all of parents of this node
 */
static void MakeNodePortal (node_t *node)
{
	portal_t *new_portal, *p;
	winding_t *w;
	vec3_t normal;
	float dist = 0.0f;
	int side = 0;

	w = BaseWindingForNode(node);

	/* clip the portal by all the other portals in the node */
	for (p = node->portals; p && w; p = p->next[side]) {
		if (p->nodes[0] == node) {
			side = 0;
			VectorCopy(p->plane.normal, normal);
			dist = p->plane.dist;
		} else if (p->nodes[1] == node) {
			side = 1;
			VectorSubtract(vec3_origin, p->plane.normal, normal);
			dist = -p->plane.dist;
		} else
			Sys_Error("CutNodePortals_r: mislinked portal");

		ChopWindingInPlace(&w, normal, dist, 0.1);
	}

	if (!w)
		return;

	if (WindingIsTiny(w)) {
		c_tinyportals++;
		FreeWinding(w);
		return;
	}

	new_portal = AllocPortal();
	new_portal->plane = mapplanes[node->planenum];
	new_portal->onnode = node;
	new_portal->winding = w;
	AddPortalToNodes(new_portal, node->children[0], node->children[1]);
}


/**
 * @brief Move or split the portals that bound node so that the node's
 * children have portals instead of node.
 */
static void SplitNodePortals (node_t *node)
{
	portal_t *p, *next_portal, *new_portal;
	node_t *f, *b, *other_node;
	int side = 0;
	plane_t *plane;
	winding_t *frontwinding, *backwinding;

	plane = &mapplanes[node->planenum];
	f = node->children[0];
	b = node->children[1];

	for (p = node->portals; p; p = next_portal) {
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
			side = 1;
		else
			Sys_Error("SplitNodePortals: mislinked portal");
		next_portal = p->next[side];

		other_node = p->nodes[!side];
		RemovePortalFromNode(p, p->nodes[0]);
		RemovePortalFromNode(p, p->nodes[1]);

		/* cut the portal into two portals, one on each side of the cut plane */
		ClipWindingEpsilon(p->winding, plane->normal, plane->dist,
			SPLIT_WINDING_EPSILON, &frontwinding, &backwinding);

		if (frontwinding && WindingIsTiny(frontwinding)) {
			FreeWinding(frontwinding);
			frontwinding = NULL;
			c_tinyportals++;
		}

		if (backwinding && WindingIsTiny(backwinding)) {
			FreeWinding(backwinding);
			backwinding = NULL;
			c_tinyportals++;
		}

		if (!frontwinding && !backwinding) {	/* tiny windings on both sides */
			continue;
		}

		if (!frontwinding) {
			FreeWinding(backwinding);
			if (side == 0)
				AddPortalToNodes(p, b, other_node);
			else
				AddPortalToNodes(p, other_node, b);
			continue;
		}
		if (!backwinding) {
			FreeWinding(frontwinding);
			if (side == 0)
				AddPortalToNodes(p, f, other_node);
			else
				AddPortalToNodes(p, other_node, f);
			continue;
		}

		/* the winding is split */
		new_portal = AllocPortal();
		*new_portal = *p;
		new_portal->winding = backwinding;
		FreeWinding(p->winding);
		p->winding = frontwinding;

		if (side == 0) {
			AddPortalToNodes(p, f, other_node);
			AddPortalToNodes(new_portal, b, other_node);
		} else {
			AddPortalToNodes(p, other_node, f);
			AddPortalToNodes(new_portal, other_node, b);
		}
	}

	node->portals = NULL;
}


static void CalcNodeBounds (node_t *node)
{
	portal_t *p;
	int s, i;

	/* calc mins/maxs for both leafs and nodes */
	ClearBounds(node->mins, node->maxs);
	for (p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);
		if (!p->winding)
			continue;
		for (i = 0; i < p->winding->numpoints; i++)
			AddPointToBounds(p->winding->p[i], node->mins, node->maxs);
	}
}


static void MakeTreePortals_r (node_t *node)
{
	int i;

	CalcNodeBounds(node);
	if (node->mins[0] >= node->maxs[0]) {
		Com_Printf("WARNING: node without a volume\n");
	}

	for (i = 0; i < 3; i++) {
		if (node->mins[i] < -MAX_WORLD_WIDTH || node->maxs[i] > MAX_WORLD_WIDTH) {
			Com_Printf("WARNING: node with unbounded volume %i\n", (int)node->mins[i]);
			break;
		}
	}
	if (node->planenum == PLANENUM_LEAF)
		return;

	MakeNodePortal(node);
	SplitNodePortals(node);

	MakeTreePortals_r(node->children[0]);
	MakeTreePortals_r(node->children[1]);
}

void MakeTreePortals (tree_t *tree)
{
	MakeHeadnodePortals(tree);
	MakeTreePortals_r(tree->headnode);
}

/**
 * @brief Finds a brush side to use for texturing the given portal
 */
static void FindPortalSide (portal_t *p)
{
	uint32_t viscontents;
	bspbrush_t *bb;
	int i, j, planenum;
	side_t *bestside;
	float bestdot;

	/* decide which content change is strongest
	 * solid > water, etc */
	viscontents = VisibleContents(p->nodes[0]->contentFlags ^ p->nodes[1]->contentFlags);
	if (!viscontents)
		return;

	planenum = p->onnode->planenum;
	bestside = NULL;
	bestdot = 0;

	for (j = 0; j < 2; j++) {
		const node_t *n = p->nodes[j];
		const plane_t *p1 = &mapplanes[p->onnode->planenum];
		for (bb = n->brushlist; bb; bb = bb->next) {
			const mapbrush_t *brush = bb->original;

			if (!(brush->contentFlags & viscontents))
				continue;
			for (i = 0; i < brush->numsides; i++) {
				side_t *side = &brush->original_sides[i];
				float dot;
				const plane_t *p2;

				if (side->bevel)
					continue;
				/* non-visible */
				if (side->texinfo == TEXINFO_NODE)
					continue;
				/* exact match */
				if ((side->planenum &~ 1) == planenum) {
					bestside = &brush->original_sides[i];
					goto gotit;
				}
				/* see how close the match is */
				p2 = &mapplanes[side->planenum &~ 1];
				dot = DotProduct(p1->normal, p2->normal);
				if (dot > bestdot) {
					bestdot = dot;
					bestside = side;
				}
			}
		}
	}

gotit:
	if (!bestside)
		Verb_Printf(VERB_EXTRA, "WARNING: side not found for portal\n");

	p->sidefound = qtrue;
	p->side = bestside;
}


static void MarkVisibleSides_r (node_t *node)
{
	portal_t *p;
	int s;

	if (node->planenum != PLANENUM_LEAF) {
		MarkVisibleSides_r(node->children[0]);
		MarkVisibleSides_r(node->children[1]);
		return;
	}

	/* empty leafs are never boundary leafs */
	if (!node->contentFlags)
		return;

	/* see if there is a visible face */
	for (p = node->portals; p; p = p->next[!s]) {
		s = (p->nodes[0] == node);
		if (!p->onnode)
			continue;		/* edge of world */
		if (!p->sidefound)
			FindPortalSide(p);
		if (p->side)
			p->side->visible = qtrue;
	}
}

void MarkVisibleSides (tree_t *tree, int startbrush, int endbrush)
{
	int i;

	Verb_Printf(VERB_EXTRA, "--- MarkVisibleSides ---\n");

	/* clear all the visible flags */
	for (i = startbrush; i < endbrush; i++) {
		mapbrush_t *mb = &mapbrushes[i];
		const int numsides = mb->numsides;
		int j;

		for (j = 0; j < numsides; j++)
			mb->original_sides[j].visible = qfalse;
	}

	/* set visible flags on the sides that are used by portals */
	MarkVisibleSides_r(tree->headnode);
}
