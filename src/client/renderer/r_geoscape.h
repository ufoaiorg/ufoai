/**
 * @file r_geoscape.h
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

#ifndef R_GEOSCAPE_H
#define R_GEOSCAPE_H

void R_Draw3DGlobe(int x, int y, int w, int h, int day, int second, const vec3_t rotate, float zoom, const char *map, qboolean disableSolarRender, float ambient, qboolean overlayNation, qboolean overlayXVI, qboolean overlayRadar, image_t *r_xviTexture, image_t *r_radarTexture, qboolean renderNationGlow);
void R_Draw2DMapMarkers(const vec2_t screenPos, float direction, const char *model, int skin);
void R_Draw3DMapMarkers(int x, int y, int w, int h, const vec3_t rotate, const vec2_t pos, float direction, float earthRadius, const char *model, int skin);
void R_DrawFlatGeoscape(int x, int y, int w, int h, float p, float cx, float cy, float iz, const char *map, qboolean overlayNation, qboolean overlayXVI, qboolean overlayRadar, image_t *r_dayandnightTexture, image_t *r_xviTexture, image_t *r_radarTexture);
void R_DrawBloom(void);

#endif
