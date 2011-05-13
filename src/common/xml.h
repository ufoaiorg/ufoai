/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include <mxml.h>
#define xmlNode_t mxml_node_t
#include "../shared/mathlib.h"
#include "../shared/ufotypes.h"

void XML_AddString(xmlNode_t *parent, const char *name, const char *value);
void XML_AddBool(xmlNode_t *parent, const char *name, qboolean value);
void XML_AddFloat(xmlNode_t *parent, const char *name, float value);
void XML_AddDouble(xmlNode_t *parent, const char *name, double value);
void XML_AddByte(xmlNode_t *parent, const char *name, byte value);
void XML_AddShort(xmlNode_t *parent, const char *name, short value);
void XML_AddInt(xmlNode_t *parent, const char *name, int value);
void XML_AddLong(xmlNode_t *parent, const char *name, long value);
void XML_AddPos3(xmlNode_t *parent, const char *name, const vec3_t pos);
void XML_AddPos2(xmlNode_t *parent, const char *name, const vec2_t pos);
void XML_AddDate(xmlNode_t *parent, const char *name, const int day, const int sec);

void XML_AddStringValue(xmlNode_t *parent, const char *name, const char *value);
void XML_AddBoolValue(xmlNode_t *parent, const char *name, qboolean value);
void XML_AddFloatValue(xmlNode_t *parent, const char *name, float value);
void XML_AddDoubleValue(xmlNode_t *parent, const char *name, double value);
void XML_AddByteValue(xmlNode_t *parent, const char *name, byte value);
void XML_AddShortValue(xmlNode_t *parent, const char *name, short value);
void XML_AddIntValue(xmlNode_t *parent, const char *name, int value);
void XML_AddLongValue(xmlNode_t *parent, const char *name, long value);

xmlNode_t * XML_AddNode(xmlNode_t *parent, const char *name);

qboolean XML_GetBool(xmlNode_t *parent, const char *name, const qboolean defaultval);
int XML_GetInt(xmlNode_t *parent, const char *name, const int defaultval);
short XML_GetShort(xmlNode_t *parent, const char *name, const short defaultval);
long XML_GetLong(xmlNode_t *parent, const char *name, const long defaultval);
const char * XML_GetString(xmlNode_t *parent, const char *name);
float XML_GetFloat(xmlNode_t *parent, const char *name, const float defaultval);
double XML_GetDouble(xmlNode_t *parent, const char *name, const double defaultval);
xmlNode_t * XML_GetPos2(xmlNode_t *parent, const char *name, vec2_t pos);
xmlNode_t * XML_GetNextPos2(xmlNode_t *actual, xmlNode_t *parent, const char *name, vec2_t pos);
xmlNode_t * XML_GetPos3(xmlNode_t *parent, const char *name, vec3_t pos);
xmlNode_t * XML_GetNextPos3(xmlNode_t *actual, xmlNode_t *parent, const char *name, vec3_t pos);
xmlNode_t * XML_GetDate(xmlNode_t *parent, const char *name, int *day, int *sec);

xmlNode_t * XML_GetNode(xmlNode_t *parent, const char *name);
xmlNode_t * XML_GetNextNode(xmlNode_t *current, xmlNode_t *parent, const char *name);

xmlNode_t *XML_Parse(const char *buffer);

#endif
