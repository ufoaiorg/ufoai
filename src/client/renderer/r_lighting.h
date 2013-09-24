/**
 * @file
 */

/*
 Copyright(c) 1997-2001 Id Software, Inc.
 Copyright(c) 2002 The Quakeforge Project.
 Copyright(c) 2006 Quake2World.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or(at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#pragma once

/** @brief lighting structure which contains static and dynamic lighting info for entities */
typedef struct lighting_s {
	/* Cache */
	vec3_t lastCachePos;	/**< Static light interaction was last calculated for this coords */
	const light_t* cachedLights[MAX_ENTITY_LIGHTS]; /**< cached static lights */
	int numCachedLights;	/**< How many static lights are in cache */

	/* Lights */
	const light_t* lights[MAX_ENTITY_LIGHTS]; /**< static and dynamic lights sorted by distance */
	int numLights;
	int lastLitFrame;		/**< renderer frame for which those lights were calculated */

	/* Shadows */
	bool inShadow;			/**< true if entity is shadowed from the sun */
	/* Stencil shadow (not necessarily from the sun) */
	vec3_t shadowOrigin;
	int lastShadowedFrame;	/**< renderer frame for which this origin was calculated */
} lighting_t;

#define LIGHTING_MAX_SHADOW_DISTANCE 128.0
#define CACHE_CLEAR_TRESHOLD 2.0
