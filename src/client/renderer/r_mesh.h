/**
 * @file r_mesh.c
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

#ifndef R_MESH_H
#define R_MESH_H

void R_DrawModelDirect(modelInfo_t *mi, modelInfo_t *pmi, const char *tagname);
void R_ModelAutoScale(const vec2_t boxSize, modelInfo_t *mi, vec3_t scale, vec3_t center);
int R_GetTagIndexByName(const model_t* mod, const char* tagName);
void R_GetTags(const model_t* mod, const char* tagName, int currentFrame, int oldFrame, const mAliasTagOrientation_t **current, const mAliasTagOrientation_t **old);
qboolean R_GetTagMatrix(const model_t* mod, const char* tagName, int frame, float matrix[16]);

#endif
