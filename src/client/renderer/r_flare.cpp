/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

void R_CreateSurfaceFlare (mBspSurface_t* surf)
{
	material_t* m = &surf->texinfo->image->material;

	if (!(m->flags & STAGE_FLARE)) /* surface is not flared */
		return;

	surf->flare = Mem_PoolAllocType(mBspFlare_t, vid_modelPool);

	/* move the flare away from the surface, into the level */
	VectorMA(surf->center, 2, surf->normal, surf->flare->origin);

	/* calculate the flare radius based on surface size */
	vec3_t span;
	surf->mbsBox.getDiagonal(span);
	surf->flare->radius = VectorLength(span);

	const materialStage_t* s = m->stages; /* resolve the flare stage */
	for (;;) {
		if (s->flags & STAGE_FLARE)
			break;
		s = s->next;
	}

	/* resolve flare color */
	if (s->flags & STAGE_COLOR)
		VectorCopy(s->color, surf->flare->color);
	else
		VectorCopy(surf->lightColor, surf->flare->color);

	/* and scaled radius */
	if (s->flags & (STAGE_SCALE_S | STAGE_SCALE_T))
		surf->flare->radius *= (s->scale.s ? s->scale.s : s->scale.t);

	/* and image */
	surf->flare->image = s->image;
}

/**
 * @brief Flares are batched by their texture.  Usually, this means one draw operation
 * for all flares in view.  Flare visibility is calculated every few millis, and
 * flare alpha is ramped up or down depending on the results of the visibility
 * trace.  Flares are also faded according to the angle of their surface to the
 * view origin.
 */
void R_DrawFlareSurfaces (const mBspSurfaces_t* surfs, glElementIndex_t* indexPtr)
{
	if (!r_flares->integer)
		return;

	if (!surfs->count)
		return;

	bool oldblend = r_state.blend_enabled;

	R_EnableColorArray(true);

	R_ResetArrayState();

	/** @todo better GL state handling */
	glDisable(GL_DEPTH_TEST);
	R_EnableBlend(true);
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE);

	const image_t* image = r_flaretextures[0];
	R_BindTexture(image->texnum);

	int i, j, k, l;
	j = k = l = 0;
	for (i = 0; i < surfs->count; i++) {
		vec3_t view, verts[4];
		vec3_t right, up, upright, downright;
		float dot, dist, scale, alpha;
		bool visible;
		mBspSurface_t* surf = surfs->surfaces[i];
		mBspFlare_t* f;

		if (surf->frame != r_locals.frame)
			continue;

		f = surf->flare;

		/* bind the flare's texture */
		if (f->image != image) {
			R_DrawArrays(0, l / 3);
			j = k = l = 0;

			refdef.batchCount++;

			image = f->image;
			R_BindTexture(image->texnum);
		}

		/* periodically test visibility to ramp alpha */
		if (refdef.time - f->time > 0.02) {
			if (refdef.time - f->time > 0.5) /* reset old flares */
				f->alpha = 0;

			R_Trace(refdef.viewOrigin, f->origin, 0, MASK_SOLID);
			visible = refdef.trace.fraction == 1.0;

			f->alpha += (visible ? 0.03 : -0.15); /* ramp */

			if (f->alpha > 0.75) /* clamp */
				f->alpha = 0.75;
			else if (f->alpha < 0)
				f->alpha = 0.0;

			f->time = refdef.time;
		}

		VectorSubtract(f->origin, refdef.viewOrigin, view);
		dist = VectorNormalize(view);

		/* fade according to angle */
		dot = DotProduct(surf->normal, view);
		if (dot > 0)
			continue;

		alpha = 0.1 + -dot * r_flares->value;

		if (alpha > 1.0)
			alpha = 1.0;

		alpha = f->alpha * alpha;

		/* scale according to distance */
		scale = f->radius + (f->radius * dist * .0005);

		VectorScale(r_locals.right, scale, right);
		VectorScale(r_locals.up, scale, up);

		VectorAdd(up, right, upright);
		VectorSubtract(right, up, downright);

		VectorSubtract(f->origin, downright, verts[0]);
		VectorAdd(f->origin, upright, verts[1]);
		VectorAdd(f->origin, downright, verts[2]);
		VectorSubtract(f->origin, upright, verts[3]);

		for (int m = 0; m < 4; m++) { /* duplicate color data to all 4 verts */
			memcpy(&r_state.color_array[j], f->color, sizeof(vec3_t));
			r_state.color_array[j + 3] = alpha;
			j += 4;
		}

		/* copy texcoord info */
		memcpy(&texunit_diffuse.texcoord_array[k], default_texcoords, sizeof(vec2_t) * 4);
		k += sizeof(vec2_t) / sizeof(vec_t) * 4;

		/* and lastly copy the 4 verts */
		memcpy(&r_state.vertex_array_3d[l], verts, sizeof(vec3_t) * 4);
		l += sizeof(vec3_t) / sizeof(vec_t) * 4;
	}

	R_DrawArrays(0, l / 3);

	refdef.batchCount++;

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	R_EnableBlend(oldblend);
	glEnable(GL_DEPTH_TEST);

	R_EnableColorArray(false);

	R_Color(nullptr);
}
