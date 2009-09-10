/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#ifndef CLIENT_MXML_UFOAI_H
#define CLIENT_MXML_UFOAI_H

#include "mxml.h"
#include "../../shared/mathlib.h"
#include "../../shared/ufotypes.h"

void mxml_AddString(mxml_node_t *parent, const char *name, const char *value);
void mxml_AddBool(mxml_node_t *parent, const char *name, qboolean value);
void mxml_AddFloat(mxml_node_t *parent, const char *name, float value);
void mxml_AddDouble(mxml_node_t *parent, const char *name, double value);
void mxml_AddByte(mxml_node_t *parent, const char *name, byte value);
void mxml_AddShort(mxml_node_t *parent, const char *name, short value);
void mxml_AddInt(mxml_node_t *parent, const char *name, int value);
void mxml_AddLong(mxml_node_t *parent, const char *name, long value);
void mxml_AddPos3(mxml_node_t *parent, const char *name, const vec3_t pos);
void mxml_AddPos2(mxml_node_t *parent, const char *name, const vec2_t pos);
mxml_node_t * mxml_AddNode(mxml_node_t *parent, const char *name);

qboolean mxml_GetBool(mxml_node_t *parent, const char *name, const qboolean defaultval);
int mxml_GetInt(mxml_node_t *parent, const char *name, const int defaultval);
short mxml_GetShort(mxml_node_t *parent, const char *name, const short defaultval);
long mxml_GetLong(mxml_node_t *parent, const char *name, const long defaultval);
const char * mxml_GetString(mxml_node_t *parent, const char *name);
float mxml_GetFloat(mxml_node_t *parent, const char *name, const float defaultval);
double mxml_GetDouble(mxml_node_t *parent, const char *name, const double defaultval);
mxml_node_t * mxml_GetPos2(mxml_node_t *parent, const char *name, vec2_t pos);
mxml_node_t * mxml_GetNextPos2(mxml_node_t *actual, mxml_node_t *parent, const char *name, vec2_t pos);
mxml_node_t * mxml_GetPos3(mxml_node_t *parent, const char *name, vec3_t pos);
mxml_node_t * mxml_GetNextPos3(mxml_node_t *actual, mxml_node_t *parent, const char *name, vec3_t pos);
mxml_node_t * mxml_GetNode(mxml_node_t *parent, const char *name);
mxml_node_t * mxml_GetNextNode(mxml_node_t *current, mxml_node_t *parent, const char *name);

mxml_type_t mxml_ufo_type_cb(mxml_node_t *node);

#endif
