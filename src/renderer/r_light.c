/**
 * @file r_light.c
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
#include "r_light.h"
#include "r_entity.h"

#define LIGHT_RADIUS_FACTOR 100.0

int r_numLights;
static light_t r_lightsArray[MAX_GL_LIGHTS];

void R_AddLight (vec3_t origin, float radius, vec3_t color)
{
	int i;

	if (!r_light->integer)
		return;

	if (r_numLights == MAX_GL_LIGHTS)
		return;

	i = r_numLights++;

	VectorCopy(origin, r_lightsArray[i].origin);
	r_lightsArray[i].radius = radius;
	VectorCopy(color, r_lightsArray[i].color);
}

void R_EnableLights (int mask)
{
	light_t *l;
	vec4_t position;
	vec4_t diffuse;
	int i, j;

	position[3] = diffuse[3] = 1.0;
	j = 0;

	if (mask) {  /* enable up to 8 light sources */
		for (i = 0, l = r_lightsArray; i < r_numLights; i++, l++) {
			if (j == MAX_GL_LIGHTS)
				break;

			if (mask & (1 << i)) {
				VectorCopy(l->origin, position);
				glLightfv(GL_LIGHT0 + j, GL_POSITION, position);

				VectorCopy(l->color, diffuse);
				glLightfv(GL_LIGHT0 + j, GL_DIFFUSE, diffuse);

				glLightf(GL_LIGHT0 + j, GL_CONSTANT_ATTENUATION, l->radius * LIGHT_RADIUS_FACTOR);
				j++;
			}
		}
	}

	for (i = j; i < MAX_GL_LIGHTS; i++)  /* disable the rest */
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, 0.0);
}

static void R_MarkLights_ (light_t *light, const vec3_t trans, int bit, mBspNode_t *node, int tile)
{
	mBspSurface_t *surf;
	vec3_t origin;
	float dist;
	int i;

	if (node->contents != LEAFNODE)  /* leaf */
		return;

	VectorSubtract(light->origin, trans, origin);

	dist = DotProduct(origin, node->plane->normal) - node->plane->dist;

	if (dist > light->radius * LIGHT_RADIUS_FACTOR){  /* front only */
		R_MarkLights_(light, trans, bit, node->children[0], tile);
		return;
	}

	if (dist < light->radius * -LIGHT_RADIUS_FACTOR){  /* back only */
		R_MarkLights_(light, trans, bit, node->children[1], tile);
		return;
	}

	/* mark all surfaces in this node */
	surf = r_mapTiles[tile]->bsp.surfaces + node->firstsurface;

	for (i = 0; i < node->numsurfaces; i++, surf++) {
		if (surf->lightframe != r_locals.frame){  /* reset it */
			surf->lightframe = r_locals.frame;
			surf->lights = 0;
		}

		surf->lights |= bit;  /* add this light */
	}

	/* now go down both sides */
	R_MarkLights_(light, trans, bit, node->children[0], tile);
	R_MarkLights_(light, trans, bit, node->children[1], tile);
}

/**
 * @sa R_GetLevelSurfaceLists
 * @param[in] tile The maptile (map assembly)
 * @sa R_ModLoadNodes about pathfinding nodes
 */
static void R_RecurseWorldForLight (light_t *light, const vec3_t origin, int lightmask, mBspNode_t *node, int tile)
{
	/* skip special pathfinding nodes */
	if (!node->plane) {
		R_RecurseWorldForLight(light, origin, lightmask, node->children[0], tile);
		R_RecurseWorldForLight(light, origin, lightmask, node->children[1], tile);
	} else {
		R_MarkLights_(light, vec3_origin, lightmask, node, tile);
	}
}

void R_MarkLights (void)
{
	int i, j, k;
	int tile;
	const int mask = 1 << refdef.worldlevel;

	/* flag all surfaces for each light source */
	for (i = 0; i < r_numLights; i++) {
		light_t *light = &r_lightsArray[i];

		for (tile = 0; tile < r_numMapTiles; tile++) {
			/* don't draw weaponclip, actorclip and stepon */
			for (k = 0; k <= LEVEL_LASTVISIBLE; k++) {
				/* check the worldlevel flags */
				if (k && !(k & mask))
					continue;

				if (!r_mapTiles[tile]->bsp.submodels[k].numfaces)
					continue;

				/* world surfaces */
				R_RecurseWorldForLight(light, vec3_origin, 1 << i,
					r_mapTiles[tile]->bsp.nodes + r_mapTiles[tile]->bsp.submodels[k].headnode, tile);
			}

			/* and bsp entity surfaces */
			for (j = 0; j < r_numEntities; j++) {
				entity_t *e = R_GetEntity(j);

				if (e->model && e->model->type == mod_bsp_submodel)
					R_RecurseWorldForLight(light, e->origin, 1 << i, e->model->bsp.nodes, tile);
			}
		}
	}

	/* clear all hardware lights */
	for (i = 0; i < MAX_GL_LIGHTS; i++)
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, 0);
}

void R_EnableLightsByRadius (const vec3_t p)
{
	const light_t *l;
	vec3_t delta;
	int i, mask;

	mask = 0;

	for (i = 0, l = r_lightsArray; i < r_numLights; i++, l++) {
		VectorSubtract(l->origin, p, delta);

		if (VectorLength(delta) < l->radius * LIGHT_RADIUS_FACTOR)
			mask |= (1 << i);
	}

	R_EnableLights(mask);
}
