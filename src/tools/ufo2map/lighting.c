/**
 * @file lighting.c
 * @note every surface must be divided into at least two patches each axis
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

#include "lighting.h"
#include "map.h"
#include "bsp.h"

/**
 * @brief Build the lightmap out of light entities and surface lights (patches)
 * @note Call @c CalcTextureReflectivity before entering this function
 */
void LightWorld (void)
{
	if (curTile->numnodes == 0 || curTile->numfaces == 0)
		Sys_Error("Empty map");

	/* initialize light data */
	curTile->lightdata[config.compile_for_day][0] = config.lightquant;
	curTile->lightdatasize[config.compile_for_day] = 1;

	MakeTracingNodes(LEVEL_LASTLIGHTBLOCKING + 1);

	/* turn each face into a single patch */
	BuildPatches();

	/* subdivide patches to a maximum dimension */
	SubdividePatches();

	/* create lights out of patches and lights */
	BuildLights();

	/* patches are no longer needed */
	FreePatches();

	/* build per-vertex normals for phong shading */
	BuildVertexNormals();

	/* build initial facelights */
	RunThreadsOn(BuildFacelights, curTile->numfaces, config.verbosity >= VERB_NORMAL, "FACELIGHTS");

	/* finalize it and write it out */
	RunThreadsOn(FinalLightFace, curTile->numfaces, config.verbosity >= VERB_NORMAL, "FINALLIGHT");
	CloseTracingNodes();
}
