/*
Copyright (C) 2002-2009 UFO: Alien Invasion team.

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
#if 0	
	mxml_node_t *t;
	t = mxmlNewElement(parent, name);
	mxmlElementSetAttr(t,"type", "string");
	/*mxmlElementSetAttr(t,"value", value);*/
	mxmlNewOpaque(t, value);
#endif
	mxmlElementSetAttr(parent,name, value);
}

void mxml_AddBool (mxml_node_t *parent, const char *name, qboolean value)
{
#if 0	
	mxml_node_t *t;
	t = mxmlNewElement(parent, name);
	mxmlElementSetAttr(t,"type", "int");
	mxmlNewInteger(t, value?1:0);
#endif
	mxmlElementSetAttr(parent,name, value?"1":"0");
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
#if 0	
	mxml_node_t *t;
	t = mxmlNewElement(parent, name);
	mxmlElementSetAttr(t,"type", "double");
	mxmlNewReal(t, value);
#endif
}

void mxml_AddByte (mxml_node_t *parent, const char *name, byte value)
{
	mxml_AddLong(parent, name, value);
#if 0	
	mxml_node_t *t;
	t = mxmlNewElement(parent, name);
	mxmlElementSetAttr(t,"type", "int");
	mxmlNewInteger(t, value);
#endif
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
	snprintf(txt,sizeof(txt), "%ld", value);
	mxmlElementSetAttr(parent,name, txt);
#if 0
	mxml_node_t *t;
	t = mxmlNewElement(parent, name);
	mxmlElementSetAttr(t,"type", "long");
	mxmlNewLong(t, value);
#endif
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
	txt = mxmlElementGetAttr(parent,name);
	if (!txt)
		return defaultval;
	
	return (atoi(txt)!=0);
}

int mxml_GetInt (mxml_node_t *parent, const char *name, const int defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent,name);
	if (!txt)
		return defaultval;
	return atoi(txt);
}
short mxml_GetShort (mxml_node_t *parent, const char *name, const short defaultval)
{	
	const char *txt;
	txt = mxmlElementGetAttr(parent,name);
	if (!txt)
		return defaultval;
	return atoi(txt);
}
long mxml_GetLong (mxml_node_t *parent, const char *name, const long defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent,name);
	if (!txt)
		return defaultval;
	return atol(txt);
}
const char * mxml_GetString (mxml_node_t *parent, const char *name)
{
	const char *str = mxmlElementGetAttr(parent,name);
	if (!str)
		return "";
	return str;
}

float mxml_GetFloat (mxml_node_t *parent, const char *name, const float defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent,name);
	if (!txt)
		return defaultval;
	return atof(txt);
}

double mxml_GetDouble (mxml_node_t *parent, const char *name, const double defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent,name);
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
	mxml_node_t *p = mxml_GetNextNode(actual,parent, name);
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
#if 0

mxml_node_t * mxml_GetBoolean(mxml_node_t *parent, const char *name, qboolean * value, const qboolean defaultval)
{	/*Searching for the right node*/
	int s;
	mxml_node_t *node = mxml_GetInt(parent, name, &s, defaultval?1:0);
	*value = (s==1);
	return node;
}
/** 
 * @brief reads an integer value from the actual parent node.
 */
mxml_node_t * mxml_GetInt (mxml_node_t *parent, const char *name, int *value, const int defaultval)
{
	/*Searching for the right node*/
	mxml_node_t * node;
	long i;
	/* All Values will be saved as long values, for better restore capabilities */
	node = mxml_GetLong(parent, name, &i, defaultval);
	*value = i;
	return node;
}



mxml_node_t * mxml_GetNextInt (mxml_node_t *actual, mxml_node_t *parent, const char * name, int *value, const int defaultval)
{
	mxml_node_t *node;
	long i;
	/* All Values will be saved as long values, for better restore capabilities */
	node = mxml_GetNextLong(actual, parent, name, &i, defaultval);
	*value = i;
	return node;
}
#if 0
mxml_node_t * mxml_GetByte (mxml_node_t *parent, const char *name, byte *value, const byte defaultval)
{
	/*Searching for the right node*/
	byte s;
	mxml_node_t *node = mxmlFindElement(parent, parent, name, "type", "int", MXML_DESCEND_FIRST);
	if (!node)
	{
		*value=defaultval;
		return NULL;
	}
	if (!node->child)
	{
		*value=defaultval;
		return NULL;
	}
	if (node->child->type != MXML_INTEGER)
	{
		*value=defaultval;
		return NULL;
	}
	s= node->child->value.integer;
	*value = s;
	return node;
}

mxml_node_t * mxml_GetNextByte (mxml_node_t *actual, mxml_node_t *parent, const char * name, byte *value, const byte defaultval)
{
	mxml_node_t *node;
	byte s;
	node = mxmlFindElement(actual, parent, name, "type", "int", MXML_NO_DESCEND);
	if (!node){
		*value= defaultval;
		return NULL;
	}
	if (!node->child){
		*value=defaultval;
		return NULL;
	}
	if (node->child->type != MXML_INTEGER)
	{
		*value=defaultval;
		return NULL;
	}
	s = node->child->value.integer;
	*value=s;
	return node;
}
#endif
mxml_node_t * mxml_GetShort (mxml_node_t *parent, const char *name, short *value, const short defaultval)
{
	/*Searching for the right node*/
	long s;
	mxml_node_t *node = mxml_GetLong(parent, name, &s, defaultval);
	*value=s;
	return node;
}

mxml_node_t * mxml_GetNextShort (mxml_node_t *actual, mxml_node_t *parent, const char * name, short *value, const short defaultval)
{
	mxml_node_t *node;
	long s;
	node = mxml_GetNextLong(actual, parent, name, &s, defaultval);
	*value=s;
	return node;
}

mxml_node_t * mxml_GetLong(mxml_node_t *parent, const char *name, long * value, const long defaultval)
{
	/*Searching for the right node*/
	mxml_node_t *node = mxmlFindElement(parent, parent, name, "type", "long", MXML_DESCEND_FIRST);
	if (!node)
	{
		*value=defaultval;
		return NULL;
	}
	if (!node->child)
	{
		*value=defaultval;
		return NULL;
	}
	if (node->child->type != MXML_LONG)
	{
		*value=defaultval;
		return NULL;
	}
	*value = node->child->value.longint;
	return node;
}

mxml_node_t * mxml_GetNextLong(mxml_node_t *actual, mxml_node_t *parent, const char *name, long *value, const long defaultval)
{
	mxml_node_t *node;
	node = mxmlFindElement(actual, parent, name, "type", "int", MXML_NO_DESCEND);
	if (!node)
	{
		*value=defaultval;
		return NULL;
	}
	if (!node->child)
	{
		*value=defaultval;
		return NULL;
	}
	if (node->child->type != MXML_LONG)
	{
		*value=defaultval;
		return NULL;
	}
	*value = node->child->value.longint;
	return node;
}

const char * mxml_GetString (mxml_node_t *parent, const char *name)
{
	/*Searching for the right node*/
	mxml_node_t *node = mxmlFindElement(parent, parent, name, "type", "string", MXML_DESCEND_FIRST);
	if (!node)
	{
		/*printf("No Element found\n");*/
		return NULL;
	}
	if (!node->child)
	{
		/*printf("No Child defined\n");*/
		return NULL;
	}
	if (node->child->type != MXML_OPAQUE)
	{
		/*printf("Node is of Type %d\n", node->type);*/
		return NULL;
	}
	return node->child->value.opaque;
}



mxml_node_t * mxml_GetDouble (mxml_node_t *parent, const char *name, double *value, const double defaultval)
{
	mxml_node_t *node = mxmlFindElement(parent, parent, name, "type", "double", MXML_DESCEND_FIRST);
	if (!node)
	{
		/*printf("No Element found\n");*/
		*value = defaultval;
		return NULL;
	}
	if (!node->child)
	{
		/*printf("No Child defined\n");*/
		*value = defaultval;
		return NULL;
	}
	if (node->child->type != MXML_REAL)
	{
		/*printf("Node is of Type %d\n", node->type);*/
		*value = defaultval;
		return NULL;
	}
	*value = node->child->value.real;
	return node;
}
mxml_node_t * mxml_GetFloat (mxml_node_t *parent, const char *name, float *value, const float defaultval)
{
	double f;
	mxml_node_t *node = mxml_GetDouble(parent, name, &f, defaultval);
	*value=f;
	return node;
}
#endif
/**
 * @brief callback function for parsing the node tree
 */
mxml_type_t    mxml_ufo_type_cb(mxml_node_t *node)
{
	const char *type;

     /*
	* You can lookup attributes and/or use the
	* element name, hierarchy, etc...
     */

	type = mxmlElementGetAttr(node, "type");
	if (type == NULL)
		type = node->value.element.name;

	if (!strcmp(type, "int"))
		return (MXML_INTEGER);
	else if (!strcmp(type, "opaque"))
		return (MXML_OPAQUE);
	else if (!strcmp(type, "string"))
		return (MXML_OPAQUE);
	else if (!strcmp(type, "double"))
		return (MXML_REAL);
	else
		return (MXML_TEXT);
}



