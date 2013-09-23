/**
 * @file
 * @brief surface-related refresh code
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

#include "r_local.h"
#include "r_lightmap.h"
#include "r_light.h"
#include "r_error.h"
#include "r_draw.h"

void R_SetSurfaceBumpMappingParameters (const mBspSurface_t* surf, const image_t* normalMap, const image_t* specularMap)
{
	if (!r_state.lighting_enabled)
		return;

	if (!r_bumpmap->value)
		return;

	assert(surf);

	if (normalMap && (surf->flags & MSURF_LIGHTMAP)) {
		const image_t* image = surf->texinfo->image;
		R_BindDeluxemapTexture(surf->deluxemap_texnum);
		R_EnableBumpmap(normalMap);
		R_EnableSpecularMap(specularMap, true);
		R_UseMaterial(&image->material);
	} else {
		R_EnableBumpmap(nullptr);
		R_EnableSpecularMap(nullptr, false);
		R_UseMaterial(nullptr);
	}
}

/**
 * @brief Set the surface state according to surface flags and bind the texture
 * @sa R_DrawSurfaces
 */
static void R_SetSurfaceState (const mBspSurface_t* surf)
{
	image_t* image;

	if (r_state.blend_enabled) {  /* alpha blend */
		vec4_t color = {1.0, 1.0, 1.0, 1.0};
		switch (surf->texinfo->flags & (SURF_BLEND33 | SURF_BLEND66)) {
		case SURF_BLEND33:
			color[3] = 0.33;
			break;
		case SURF_BLEND66:
			color[3] = 0.66;
			break;
		}

		R_Color(color);
	}

	image = surf->texinfo->image;
	R_BindTexture(image->texnum);  /* texture */

	if (texunit_lightmap.enabled) {  /* lightmap */
		if (surf->flags & MSURF_LIGHTMAP)
			R_BindLightmapTexture(surf->lightmap_texnum);
	}

	R_SetSurfaceBumpMappingParameters(surf, image->normalmap, image->specularmap);

	R_EnableGlowMap(image->glowmap);

	R_CheckError();
}

/**
 * @brief General surface drawing function, that draw the surface chains
 * @note The needed states for the surfaces must be set before you call this
 * @sa R_DrawSurface
 * @sa R_SetSurfaceState
 */
void R_DrawSurfaces (const mBspSurfaces_t* surfs, glElementIndex_t* indexPtr)
{
	int numSurfaces = surfs->count;
	mBspSurface_t** surfPtrList = surfs->surfaces;
	const int frame = r_locals.frame;

	int lastLightMap = 0, lastDeluxeMap = 0;
	image_t* lastTexture = nullptr;
	uint32_t lastFlags = ~0;

	int batchStart = 0, batchSize = 0; /* in triangles */

	while (numSurfaces--) {
		const mBspSurface_t* surf = *surfPtrList++;
		const int numTriangles = surf->numTriangles;
		mBspTexInfo_t* texInfo;
		int texFlags;
		bool newBatch = false;

		if (surf->frame != frame)
			continue;
		if (numTriangles <= 0)
			continue;

		refdef.brushCount += numTriangles;

		if (batchStart + batchSize != surf->firstTriangle) {
			/* Cannot continue assembling the batch, draw it and start a new one*/
			if (batchSize > 0) {
				glDrawElements(GL_TRIANGLES, batchSize * 3, GL_ELEMENT_INDEX_TYPE, indexPtr + batchStart * 3);
				refdef.batchCount++;
			}
			batchStart = surf->firstTriangle;
			batchSize = 0;
			newBatch = true;
		}

		/** @todo integrate it better with R_SetSurfaceState - maybe cache somewhere in the mBspSurface_t ? */
		texInfo = surf->texinfo;
		texFlags = texInfo->flags & (SURF_BLEND33 | SURF_BLEND66); /* should match flags that affect R_SetSurfaceState behavior */
		if (texInfo->image != lastTexture || surf->lightmap_texnum != lastLightMap || surf->deluxemap_texnum != lastDeluxeMap || texFlags != lastFlags) {
			if (!newBatch) {
				/* changes in texturing require new batch */
				glDrawElements(GL_TRIANGLES, batchSize * 3, GL_ELEMENT_INDEX_TYPE, indexPtr + batchStart * 3);
				refdef.batchCount++;
				batchStart = surf->firstTriangle;
				batchSize = 0;
			}
			lastTexture = texInfo->image;
			lastLightMap = surf->lightmap_texnum;
			lastDeluxeMap = surf->deluxemap_texnum;
			lastFlags = texFlags;
			R_SetSurfaceState(surf);
		}

		batchSize += numTriangles;
	}

	/* finish uncomplete batch, if any */
	if (batchSize > 0) {
		glDrawElements(GL_TRIANGLES, batchSize * 3, GL_ELEMENT_INDEX_TYPE, indexPtr + batchStart * 3);
		refdef.batchCount++;
	}

	/* reset state */
	if (r_state.active_normalmap)
		R_EnableBumpmap(nullptr);

	R_EnableGlowMap(nullptr);

	R_Color(nullptr);
}
