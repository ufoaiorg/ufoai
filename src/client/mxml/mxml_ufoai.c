/**
 * @file mxml_ufoai.c
 */

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

#include "mxml_ufoai.h"

void mxml_AddString (mxml_node_t *parent, const char *name, const char *value)
{
	mxmlElementSetAttr(parent, name, value);
}

void mxml_AddBool (mxml_node_t *parent, const char *name, qboolean value)
{
	mxmlElementSetAttr(parent, name, value ? "1" : "0");
}

void mxml_AddFloat (mxml_node_t *parent, const char *name, float value)
{
	mxml_AddDouble(parent, name, value);
}

void mxml_AddDouble (mxml_node_t *parent, const char *name, double value)
{
	char txt[50];
	snprintf(txt, sizeof(txt), "%f", value);
	mxmlElementSetAttr(parent, name, txt);
}

void mxml_AddByte (mxml_node_t *parent, const char *name, byte value)
{
	mxml_AddLong(parent, name, value);
}

void mxml_AddShort (mxml_node_t *parent, const char *name, short value)
{
	mxml_AddLong(parent, name, value);
}

void mxml_AddInt (mxml_node_t *parent, const char *name, int value)
{
	mxml_AddLong(parent, name, value);
}

void mxml_AddLong (mxml_node_t *parent, const char *name, long value)
{
	char txt[50];
	snprintf(txt, sizeof(txt), "%ld", value);
	mxmlElementSetAttr(parent, name, txt);
}

void mxml_AddPos3(mxml_node_t *parent, const char *name, const vec3_t pos)
{
	mxml_node_t *t;
	t = mxmlNewElement(parent, name);
	mxml_AddFloat(t, "x", pos[0]);
	mxml_AddFloat(t, "y", pos[1]);
	mxml_AddFloat(t, "z", pos[2]);
}

void mxml_AddPos2(mxml_node_t *parent, const char *name, const vec2_t pos)
{
	mxml_node_t *t;
	t = mxmlNewElement(parent, name);
	mxml_AddFloat(t, "x", pos[0]);
	mxml_AddFloat(t, "y", pos[1]);
}

mxml_node_t * mxml_AddNode(mxml_node_t *parent, const char *name)
{
	return mxmlNewElement(parent,name);
}

qboolean mxml_GetBool (mxml_node_t *parent, const char *name, const qboolean defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;

	return (atoi(txt) != 0);
}

int mxml_GetInt (mxml_node_t *parent, const char *name, const int defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;
	return atoi(txt);
}

short mxml_GetShort (mxml_node_t *parent, const char *name, const short defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;
	return atoi(txt);
}

long mxml_GetLong (mxml_node_t *parent, const char *name, const long defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;
	return atol(txt);
}

const char * mxml_GetString (mxml_node_t *parent, const char *name)
{
	const char *str = mxmlElementGetAttr(parent, name);
	if (!str)
		return "";
	return str;
}

float mxml_GetFloat (mxml_node_t *parent, const char *name, const float defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;
	return atof(txt);
}

double mxml_GetDouble (mxml_node_t *parent, const char *name, const double defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;
	return atof(txt);
}

mxml_node_t * mxml_GetPos2(mxml_node_t *parent, const char *name, vec2_t pos)
{
	mxml_node_t *p = mxml_GetNode(parent, name);
	if (!p)
		return NULL;
	pos[0] = mxml_GetFloat(p, "x", 0);
	pos[1] = mxml_GetFloat(p, "y", 0);
	return p;
}

mxml_node_t * mxml_GetNextPos2(mxml_node_t *actual,mxml_node_t *parent, const char *name, vec2_t pos)
{
	mxml_node_t *p = mxml_GetNextNode(actual, parent, name);
	if (!p)
		return NULL;
	pos[0] = mxml_GetFloat(p, "x", 0);
	pos[1] = mxml_GetFloat(p, "y", 0);
	return p;
}

mxml_node_t * mxml_GetPos3(mxml_node_t *parent, const char *name, vec3_t pos)
{
	mxml_node_t *p = mxml_GetNode(parent, name);
	if (!p)
		return NULL;
	pos[0] = mxml_GetFloat(p, "x", 0);
	pos[1] = mxml_GetFloat(p, "y", 0);
	pos[2] = mxml_GetFloat(p, "z", 0);
	return p;
}

mxml_node_t * mxml_GetNextPos3(mxml_node_t *actual, mxml_node_t *parent, const char *name, vec3_t pos)
{
	mxml_node_t *p = mxml_GetNextNode(actual, parent, name);
	if (!p)
		return NULL;
	pos[0] = mxml_GetFloat(p, "x", 0);
	pos[1] = mxml_GetFloat(p, "y", 0);
	pos[2] = mxml_GetFloat(p, "z", 0);
	return p;
}

mxml_node_t * mxml_GetNode (mxml_node_t *parent, const char *name)
{
	return mxmlFindElement(parent, parent, name, NULL, NULL, MXML_DESCEND_FIRST);
}

mxml_node_t * mxml_GetNextNode (mxml_node_t *current, mxml_node_t *parent, const char *name)
{
	return mxmlFindElement(current, parent, name, NULL, NULL, MXML_NO_DESCEND);
}

/**
 * @brief callback function for parsing the node tree
 */
mxml_type_t mxml_ufo_type_cb (mxml_node_t *node)
{
	/* You can lookup attributes and/or use the
	 * element name, hierarchy, etc... */
	const char *type = mxmlElementGetAttr(node, "type");
	if (type == NULL)
		type = node->value.element.name;

	if (!strcmp(type, "int"))
		return MXML_INTEGER;
	else if (!strcmp(type, "opaque"))
		return MXML_OPAQUE;
	else if (!strcmp(type, "string"))
		return MXML_OPAQUE;
	else if (!strcmp(type, "double"))
		return MXML_REAL;
	else
		return MXML_TEXT;
}
