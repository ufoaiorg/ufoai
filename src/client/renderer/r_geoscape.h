/**
 * @file
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

#pragma once

/** @todo these are currently duplicated - remove this defines here */
/**
 * @brief Typical zoom to use on the 3D geoscape to use same zoom values for both 2D and 3D geoscape
 * @note Used to convert openGL coordinates of the sphere into screen coordinates
 * @sa GLOBE_RADIUS */
#define STANDARD_3D_ZOOM 40.0f

#define EARTH_RADIUS 8192.0f
#define MOON_RADIUS 1024.0f
#define SUN_RADIUS 1024.0f

void R_Draw3DGlobe(const vec2_t pos, const vec2_t size, int day, int second, const vec3_t rotate, float zoom, const char* map, bool disableSolarRender, float ambient, bool overlayNation, bool overlayXVI, bool overlayRadar, image_t* r_xviTexture, image_t* r_radarTexture, bool renderNationGlow);
void R_Draw2DMapMarkers(const vec2_t screenPos, float direction, const char* model, int skin);
void R_Draw3DMapMarkers(const vec2_t nodePos, const vec2_t nodeSize, const vec3_t rotate, const vec2_t pos, float direction, float earthRadius, const char* model, int skin);
void R_DrawFlatGeoscape(const vec2_t nodePos, const vec2_t nodeSize, float p, float cx, float cy, float iz, const char* map, bool overlayNation, bool overlayXVI, bool overlayRadar, image_t* r_dayandnightTexture, image_t* r_xviTexture, image_t* r_radarTexture);
void R_DrawBloom(void);
