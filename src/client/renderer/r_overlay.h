/**
 * @file r_overlay.h
 * @brief Functions to generate and render overlay for geoscape
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

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


void R_DecreaseXVILevelEverywhere(void);
float R_ChangeXVILevel(const vec2_t pos, const int xviLevel, float factor);
void R_InitializeXVIOverlay(const byte *data);
void R_InitOverlay(void);
byte* R_GetXVIMap(int *width, int *height);
void R_InitializeRadarOverlay(qboolean source);
void R_AddRadarCoverage(const vec2_t pos, float innerRadius, float outerRadius, qboolean source);
void R_UploadRadarCoverage(void);
void R_ShutdownOverlay(void);

extern const int MAX_ALPHA_VALUE;
extern const int INITIAL_ALPHA_VALUE;
extern image_t *r_xviTexture;
extern image_t *r_radarTexture;
