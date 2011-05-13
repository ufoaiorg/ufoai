/**
 * @file xml.c
 * @brief UFO:AI interface functions to mxml
 */

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

#include "xml.h"

/**
 * @brief add a String attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 */
void XML_AddString (xmlNode_t *parent, const char *name, const char *value)
{
	if (value)
		mxmlElementSetAttr(parent, name, value);
}

/**
 * @brief add a non-empty String attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 * @note if the value is empty nothing will be added
 */
void XML_AddStringValue (xmlNode_t *parent, const char *name, const char *value)
{
	if (value && value[0] != '\0')
		mxmlElementSetAttr(parent, name, value);
}

/**
 * @brief add a Boolean attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 */
void XML_AddBool (xmlNode_t *parent, const char *name, qboolean value)
{
	mxmlElementSetAttr(parent, name, value ? "true" : "false");
}

/**
 * @brief add a non-false Boolean attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 * @note if the value is qfalse nothing will be added
 */
void XML_AddBoolValue (xmlNode_t *parent, const char *name, qboolean value)
{
	if (value)
		mxmlElementSetAttr(parent, name, value ? "true" : "false");
}

/**
 * @brief add a Float attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 */
void XML_AddFloat (xmlNode_t *parent, const char *name, float value)
{
	XML_AddDouble(parent, name, value);
}

/**
 * @brief add a non-zero Float attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 * @note if the value is zero nothing will be added
 */
void XML_AddFloatValue (xmlNode_t *parent, const char *name, float value)
{
	XML_AddDoubleValue(parent, name, value);
}

/**
 * @brief add a Double attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 */
void XML_AddDouble (xmlNode_t *parent, const char *name, double value)
{
	char txt[50];
	snprintf(txt, sizeof(txt), "%f", value);
	mxmlElementSetAttr(parent, name, txt);
}

/**
 * @brief add a non-zero Double attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 * @note if the value is zero nothing will be added
 */
void XML_AddDoubleValue (xmlNode_t *parent, const char *name, double value)
{
	char txt[50];

	if (value) {
		snprintf(txt, sizeof(txt), "%f", value);
		mxmlElementSetAttr(parent, name, txt);
	}
}

/**
 * @brief add a Byte attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 */
void XML_AddByte (xmlNode_t *parent, const char *name, byte value)
{
	XML_AddLong(parent, name, value);
}

/**
 * @brief add a non-zero Byte attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 * @note if the value is zero nothing will be added
 */
void XML_AddByteValue (xmlNode_t *parent, const char *name, byte value)
{
	XML_AddLongValue(parent, name, value);
}

/**
 * @brief add a Short attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 */
void XML_AddShort (xmlNode_t *parent, const char *name, short value)
{
	XML_AddLong(parent, name, value);
}

/**
 * @brief add a non-zero Short attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 * @note if the value is zero nothing will be added
 */
void XML_AddShortValue (xmlNode_t *parent, const char *name, short value)
{
	XML_AddLongValue(parent, name, value);
}

/**
 * @brief add an Int attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 */
void XML_AddInt (xmlNode_t *parent, const char *name, int value)
{
	XML_AddLong(parent, name, value);
}

/**
 * @brief add a non-zero Int attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 * @note if the value is zero nothing will be added
 */
void XML_AddIntValue (xmlNode_t *parent, const char *name, int value)
{
	XML_AddLongValue(parent, name, value);
}

/**
 * @brief add a Long attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 */
void XML_AddLong (xmlNode_t *parent, const char *name, long value)
{
	char txt[50];
	snprintf(txt, sizeof(txt), "%ld", value);
	mxmlElementSetAttr(parent, name, txt);
}

/**
 * @brief add a non-zero Long attribute to the XML Node
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the attribute
 * @param[in] value Value of the attribute
 * @note if the value is zero nothing will be added
 */
void XML_AddLongValue (xmlNode_t *parent, const char *name, long value)
{
	char txt[50];

	if (value) {
		snprintf(txt, sizeof(txt), "%ld", value);
		mxmlElementSetAttr(parent, name, txt);
	}
}

/**
 * @brief add a Pos3 data to the XML Tree
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the node to add
 * @param[in] pos Pos3 structure with position data
 * @note it creates a new node and adds coordinate(s) to the node as attributes
 */
void XML_AddPos3 (xmlNode_t *parent, const char *name, const vec3_t pos)
{
	xmlNode_t *t;
	t = mxmlNewElement(parent, name);
	XML_AddFloat(t, "x", pos[0]);
	XML_AddFloat(t, "y", pos[1]);
	XML_AddFloat(t, "z", pos[2]);
}

/**
 * @brief add a Pos2 data to the XML Tree
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the node to add
 * @param[in] pos Pos2 structure with position data
 * @note it creates a new node and adds coordinate(s) to the node as attributes
 */
void XML_AddPos2 (xmlNode_t *parent, const char *name, const vec2_t pos)
{
	xmlNode_t *t;
	t = mxmlNewElement(parent, name);
	XML_AddFloat(t, "x", pos[0]);
	XML_AddFloat(t, "y", pos[1]);
}

/**
 * @brief add a date data to the XML Tree
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the node to add
 * @param[in] day Day part of the date
 * @param[in] sec Second part of the date
 * @note it creates a new node and adds day/sec attributes to the node
 */
void XML_AddDate (xmlNode_t *parent, const char *name, const int day, const int sec)
{
	xmlNode_t *t;
	t = mxmlNewElement(parent, name);
	XML_AddInt(t, "day", day);
	XML_AddInt(t, "sec", sec);
}

/**
 * @brief add a new node to the XML tree
 * @param[out] parent XML Node structure to add to
 * @param[in] name Name of the new node
 * return pointer to the new XML Node structure
 */
xmlNode_t * XML_AddNode (xmlNode_t *parent, const char *name)
{
	return mxmlNewElement(parent,name);
}

/**
 * @brief retrieve a Boolean attribute from an XML Node
 * @param[in] parent XML Node structure to get from
 * @param[in] name Name of the attribute
 * @param[in] defaultval Default value to return if no such attribute defined
 */
qboolean XML_GetBool (xmlNode_t *parent, const char *name, const qboolean defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;

	if (!strcmp(txt, "true") || !strcmp(txt, "1"))
		return qtrue;
	if (!strcmp(txt, "false") || !strcmp(txt, "0"))
		return qfalse;

	return defaultval;
}

/**
 * @brief retrieve an Int attribute from an XML Node
 * @param[in] parent XML Node structure to get from
 * @param[in] name Name of the attribute
 * @param[in] defaultval Default value to return if no such attribute defined
 */
int XML_GetInt (xmlNode_t *parent, const char *name, const int defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;
	return atoi(txt);
}

/**
 * @brief retrieve a Short attribute from an XML Node
 * @param[in] parent XML Node structure to get from
 * @param[in] name Name of the attribute
 * @param[in] defaultval Default value to return if no such attribute defined
 */
short XML_GetShort (xmlNode_t *parent, const char *name, const short defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;
	return atoi(txt);
}

/**
 * @brief retrieve a Long attribute from an XML Node
 * @param[in] parent XML Node structure to get from
 * @param[in] name Name of the attribute
 * @param[in] defaultval Default value to return if no such attribute defined
 */
long XML_GetLong (xmlNode_t *parent, const char *name, const long defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;
	return atol(txt);
}

/**
 * @brief retrieve a String attribute from an XML Node
 * @param[in] parent XML Node structure to get from
 * @param[in] name Name of the attribute
 * @return empty string if no such attribute defined
 */
const char * XML_GetString (xmlNode_t *parent, const char *name)
{
	const char *str = mxmlElementGetAttr(parent, name);
	if (!str)
		return "";
	return str;
}

/**
 * @brief retrieve a Float attribute from an XML Node
 * @param[in] parent XML Node structure to get from
 * @param[in] name Name of the attribute
 * @param[in] defaultval Default value to return if no such attribute defined
 */
float XML_GetFloat (xmlNode_t *parent, const char *name, const float defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;
	return atof(txt);
}

/**
 * @brief retrieve a Double attribute from an XML Node
 * @param[in] parent XML Node structure to get from
 * @param[in] name Name of the attribute
 * @param[in] defaultval Default value to return if no such attribute defined
 */
double XML_GetDouble (xmlNode_t *parent, const char *name, const double defaultval)
{
	const char *txt;
	txt = mxmlElementGetAttr(parent, name);
	if (!txt)
		return defaultval;
	return atof(txt);
}

/**
 * @brief retrieve the first Pos2 data from an XML Node
 * @param[in] parent XML Node structure to get child from
 * @param[in] name Name of the pos node
 * @param[out] pos vec2_t structure to fill
 * @return pointer to the node the data was retrieved from
 * @return NULL if no node with name found
 */
xmlNode_t * XML_GetPos2 (xmlNode_t *parent, const char *name, vec2_t pos)
{
	xmlNode_t *p = XML_GetNode(parent, name);
	if (!p)
		return NULL;
	pos[0] = XML_GetFloat(p, "x", 0);
	pos[1] = XML_GetFloat(p, "y", 0);
	return p;
}

/**
 * @brief retrieve the next Pos2 data from an XML Node
 * @param[in] actual XML Node pointer of the previous pos data
 * @param[in] parent XML Node structure to get child from
 * @param[in] name Name of the pos node
 * @param[out] pos vec2_t structure to fill
 * @return pointer to the node the data was retrieved from
 * @return NULL if no Node with name found
 */
xmlNode_t * XML_GetNextPos2 (xmlNode_t *actual, xmlNode_t *parent, const char *name, vec2_t pos)
{
	xmlNode_t *p = XML_GetNextNode(actual, parent, name);
	if (!p)
		return NULL;
	pos[0] = XML_GetFloat(p, "x", 0);
	pos[1] = XML_GetFloat(p, "y", 0);
	return p;
}

/**
 * @brief retrieve the first Pos3 data from an XML Node
 * @param[in] parent XML Node structure to get child from
 * @param[in] name Name of the pos node
 * @param[out] pos vec3_t structure to fill
 * @return pointer to the node the data was retrieved from
 * @return NULL if no node with name found
 */
xmlNode_t * XML_GetPos3 (xmlNode_t *parent, const char *name, vec3_t pos)
{
	xmlNode_t *p = XML_GetNode(parent, name);
	if (!p)
		return NULL;
	pos[0] = XML_GetFloat(p, "x", 0);
	pos[1] = XML_GetFloat(p, "y", 0);
	pos[2] = XML_GetFloat(p, "z", 0);
	return p;
}

/**
 * @brief retrieve the next Pos3 data from an XML Node
 * @param[in] actual XML Node pointer of the previous pos data
 * @param[in] parent XML Node structure to get child from
 * @param[in] name Name of the pos node
 * @param[out] pos vec3_t structure to fill
 * @return pointer to the node the data was retrieved from
 * @return NULL if no Node with name found
 */
xmlNode_t * XML_GetNextPos3 (xmlNode_t *actual, xmlNode_t *parent, const char *name, vec3_t pos)
{
	xmlNode_t *p = XML_GetNextNode(actual, parent, name);
	if (!p)
		return NULL;
	pos[0] = XML_GetFloat(p, "x", 0);
	pos[1] = XML_GetFloat(p, "y", 0);
	pos[2] = XML_GetFloat(p, "z", 0);
	return p;
}

/**
 * @brief retrieve the date data from an XML Node
 * @param[in] parent XML Node structure to get child from
 * @param[in] name Name of the pos node
 * @param[out] day Day part of the date to fill
 * @param[out] sec Second part of the date to fill
 * @return pointer to the node the data was retrieved from
 * @return NULL if no node with name found
 */
xmlNode_t * XML_GetDate (xmlNode_t *parent, const char *name, int *day, int *sec)
{
	xmlNode_t *p = XML_GetNode(parent, name);
	if (!p)
		return NULL;
	*day = XML_GetInt(p, "day", 0);
	*sec = XML_GetInt(p, "sec", 0);
	return p;
}

/**
 * @brief Get first Node of the XML tree by name
 * @param[in] parent Parent XML Node structure
 * @param[in] name Name of the node to retrieve
 * @return pointer to the found XML Node structure or NULL
 */
xmlNode_t * XML_GetNode (xmlNode_t *parent, const char *name)
{
	return mxmlFindElement(parent, parent, name, NULL, NULL, MXML_DESCEND_FIRST);
}

/**
 * @brief Get next Node of the XML tree by name
 * @param[in] current Pointer to the previous Node was found
 * @param[in] parent Parent XML Node structure
 * @param[in] name Name of the node to retrieve
 * @return pointer to the found XML Node structure or NULL
 */
xmlNode_t * XML_GetNextNode (xmlNode_t *current, xmlNode_t *parent, const char *name)
{
	return mxmlFindElement(current, parent, name, NULL, NULL, MXML_NO_DESCEND);
}

/**
 * @brief callback function for parsing the node tree
 */
static mxml_type_t mxml_ufo_type_cb (xmlNode_t *node)
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

xmlNode_t *XML_Parse (const char *buffer)
{
	return mxmlLoadString(NULL, buffer, mxml_ufo_type_cb);
}
