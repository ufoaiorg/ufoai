/**
 * @file r_material.h
 * @brief Header file for the render material subsystem
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef R_MATERIAL_H
#define R_MATERIAL_H

/* flags will persist on the stage structures but may also bubble
 * up to the material flags to determine render eligibility */
#define STAGE_TEXTURE			(1 << 0)
#define STAGE_ENVMAP			(1 << 1)
#define STAGE_BLEND				(1 << 2)
#define STAGE_COLOR				(1 << 3)
#define STAGE_PULSE				(1 << 4)
#define STAGE_STRETCH			(1 << 5)
#define STAGE_ROTATE			(1 << 6)
#define STAGE_SCROLL_S			(1 << 7)
#define STAGE_SCROLL_T			(1 << 8)
#define STAGE_SCALE_S			(1 << 9)
#define STAGE_SCALE_T			(1 << 10)
#define STAGE_TERRAIN			(1 << 11)
#define STAGE_TAPE	(1 << 12)
#define STAGE_LIGHTMAP			(1 << 13)
#define STAGE_ANIM				(1 << 14)
#define STAGE_DIRTMAP			(1 << 15)
#define STAGE_FLARE				(1 << 16)
#define STAGE_GLOWMAPLINK		(1 << 17)

/* set on stages eligible for static, dynamic, and per-pixel lighting */
#define STAGE_LIGHTING		(1 << 30)

/* set on stages with valid render passes */
#define STAGE_RENDER		(1 << 31)

/* composite mask for simplifying state management */
#define STAGE_TEXTURE_MATRIX ( \
	STAGE_STRETCH | STAGE_ROTATE | STAGE_SCROLL_S | STAGE_SCROLL_T | \
	STAGE_SCALE_S | STAGE_SCALE_T \
)

/* frame based animation, lerp between consecutive images */
#define MAX_ANIM_FRAMES 8

#define UPDATE_THRESHOLD 0.02

typedef struct dirt_s {
	float intensity;		/**< 0.01 - 1.0 - factor for the dirt map blending (the alpha value of the color) */
} dirt_t;

typedef struct rotate_s {
	float hz, deg;
} rotate_t;

typedef struct blendmode_s {
	unsigned int src, dest;	/**< GL_ONE, GL_ZERO, ... */
} blendmode_t;

typedef struct pulse_s {
	float hz, dhz;
	float dutycycle;
} pulse_t;

typedef struct stretch_s {
	float hz, dhz;
	float amp, damp;
} stretch_t;

typedef struct scroll_s {
	float s, t;
	float ds, dt;
} scroll_t;

typedef struct scale_s {
	float s, t;
} scale_t;

typedef struct terrain_s {
	float floor;	/**< if the vertex z position is lower than this, the alpha value for the blended texture will be 0 */
	float ceil;		/**< if the vertex z position is greater than this, the alpha value will be 1 */
	float height;	/**< the z distance between floor and ceiling */
} terrain_t;

typedef struct tape_s {
	float floor;	/**< if the vertex z position is lower than this, the alpha value for the blended texture will be 0 */
	float ceil;		/**< if the vertex z position is greater than this, the alpha value will be 0 */
	float height;	/**< the z distance between floor and ceiling */
	float min;		/**< center - floor */
	float max;		/**< center + ceil */
	float center;	/**< the center where the maximum alpha value should be */
} tape_t;

typedef enum {
	ANIM_NORMAL,	/**< loops the animation from frame [0, n-1] */
	ANIM_ALTERNATE,	/**< amount of frames should be uneven [0, n-1, 1] */
	ANIM_BACKWARDS,	/**< plays the animation in reverse order. Loops from [n-1, 0] */
	ANIM_RANDOM,		/**< plays the animation in random order. */
	ANIM_RANDOMFORCE	/**< plays the animation in random order, but enforces that the following frame
						 * is not the same as the current one. */
} animLoop_t;

typedef struct anim_s {
	int num_frames;
	animLoop_t type;
	struct image_s *images[MAX_ANIM_FRAMES];
	float fps;
	float dtime;
	int dframe;
} anim_t;

typedef struct materialStage_s {
	unsigned flags;
	struct image_s *image;
	blendmode_t blend;
	vec3_t color;
	pulse_t pulse;
	stretch_t stretch;
	rotate_t rotate;
	scroll_t scroll;
	scale_t scale;
	terrain_t terrain;
	tape_t tape;
	dirt_t dirt;
	anim_t anim;
	float glowscale;
	struct materialStage_s *next;
} materialStage_t;

#define DEFAULT_BUMP 1.0f
#define DEFAULT_PARALLAX 1.0f
#define DEFAULT_SPECULAR 0.2f
#define DEFAULT_HARDNESS 0.2f
#define DEFAULT_GLOWSCALE 1.0f

typedef struct material_s {
	unsigned flags;
	float time;
	float bump;
	float parallax;
	float hardness;
	float specular;
	float glowscale;
	materialStage_t *stages;
	int num_stages;
} material_t;

extern material_t defaultMaterial;

void R_LoadMaterials(const char *map);
void R_UpdateDefaultMaterial(const char *cvarName, const char *oldValue, const char *newValue, void *data);

#endif
