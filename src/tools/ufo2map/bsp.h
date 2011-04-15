/**
 * @file bsp.h
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

#ifndef UFO2MAP_BSP_H
#define UFO2MAP_BSP_H

#include <assert.h>

#include "map.h"
#include "bspbrush.h"

#include "common/shared.h"
#include "common/scriplib.h"
#include "common/polylib.h"
#include "common/bspfile.h"

#include "../../common/tracing.h"

extern dMapTile_t *curTile;
extern mapTiles_t mapTiles;


typedef struct node_s {
	/** both leafs and nodes */
	int32_t			planenum;	/**< -1 = leaf node */
	struct node_s	*parent;
	vec3_t			mins, maxs;	/**< valid after portalization */
	bspbrush_t		*volume;	/**< one for each leaf/node */

	/** nodes only */
	side_t			*side;		/**< the side that created the node */
	struct node_s	*children[2];
	face_t			*faces;

	/** leafs only */
	bspbrush_t		*brushlist;	/**< fragments of all brushes in this leaf */
	int32_t			contentFlags;	/**< OR of all brush contents */
	int				area;		/**< for areaportals - @todo not used, remove it when we change the bsp version the next time */
	struct portal_s	*portals;	/**< also on nodes during construction */
} node_t;

typedef struct {
	struct node_s		*headnode;
	struct node_s		outside_node;
	vec3_t		mins, maxs;
} tree_t;

extern plane_t mapplanes[MAX_MAP_PLANES];
extern int nummapplanes;

extern int nummapbrushes;
extern mapbrush_t mapbrushes[MAX_MAP_BRUSHES];

extern int nummapbrushsides;
extern side_t brushsides[MAX_MAP_SIDES];
extern brush_texture_t side_brushtextures[MAX_MAP_SIDES];

extern int brush_start, brush_end;

byte GetLevelFlagsFromBrush(const mapbrush_t *brush);
void LoadMapFile(const char *filename);
void WriteMapFile(const char *filename);
uint16_t FindOrCreateFloatPlane(vec3_t normal, vec_t dist);

/*============================================================================= */

/* csg.c */

int MapBrushesBounds(const int startbrush, const int endbrush, const int level, const vec3_t clipmins, const vec3_t clipmaxs, vec3_t mins, vec3_t maxs);
bspbrush_t *MakeBspBrushList(int startbrush, int endbrush, int level, vec3_t clipmins, vec3_t clipmaxs);
bspbrush_t *ChopBrushes(bspbrush_t *head);

/* portals.c */

uint32_t VisibleContents(uint32_t contents);
void MarkVisibleSides(tree_t *tree, int start, int end);
void FreePortal(portal_t *p);
void MakeTreePortals(tree_t *tree);
void RemovePortalFromNode(portal_t *portal, node_t *l);

/*============================================================================= */

/* writebsp.c */

void SetModelNumbers(void);

void BeginBSPFile(void);
int WriteBSP(node_t *headnode);
void EndBSPFile(const char *filename);
void BeginModel(int entityNum);
void EndModel(void);
void EmitBrushes(void);
void EmitPlanes(void);

/* faces.c */

void MakeFaces(node_t *headnode);
void FixTjuncs(node_t *headnode);
int GetEdge(int v1, int v2, const face_t *f);
void FreeFace(face_t *f);

/* tree.c */

node_t *AllocNode(void);
tree_t *AllocTree(void);
tree_t *BuildTree(bspbrush_t *brushlist, vec3_t mins, vec3_t maxs);
void FreeTree(tree_t *tree);
void PruneNodes(node_t *node);

/* trace.c */

void MakeTracingNodes(int levels);
void CloseTracingNodes(void);

/* routing.c */

void DoRouting(void);

/* bsp.c */

void ProcessModels(const char *filename);

#endif /* UFO2MAP_BSP_H */
