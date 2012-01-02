/**
 * @file ui_property.h
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

#ifndef CLIENT_UI_UI_PROPERTY_H
#define CLIENT_UI_UI_PROPERTY_H

#include "ui_nodes.h"

struct value_s;
struct uiBehaviour_s;

/**
 * @brief Register a property to a behaviour.
 * It should not be used in the code
 * @param behaviour Target behaviour
 * @param name Name of the property
 * @param type Type of the property
 * @param pos position of the attribute (which store property memory) into the node structure
 * @param size size of the attribute (which store property memory) into the node structure
 * @see UI_RegisterNodeProperty
 * @see UI_RegisterExtradataNodeProperty
 * @return A link to the node property
 */
const struct value_s *UI_RegisterNodePropertyPosSize_(struct uiBehaviour_s *behaviour, const char* name, int type, int pos, int size);

/**
 * @brief Initialize a property
 * @param BEHAVIOUR behaviour Target behaviour
 * @param NAME Name of the property
 * @param TYPE Type of the property
 * @param OBJECTTYPE Object type containing the property
 * @param ATTRIBUTE Name of the attribute of the object containing data of the property
 */
#define UI_RegisterNodeProperty(BEHAVIOUR, NAME, TYPE, OBJECTTYPE, ATTRIBUTE) UI_RegisterNodePropertyPosSize_(BEHAVIOUR, NAME, TYPE, offsetof(OBJECTTYPE, ATTRIBUTE), MEMBER_SIZEOF(OBJECTTYPE, ATTRIBUTE))

/**
 * @brief Return the offset of an extradata node attribute
 * @param TYPE Extradata type
 * @param MEMBER Attribute name
 * @sa offsetof
 */
#define UI_EXTRADATA_OFFSETOF_(TYPE, MEMBER) ((size_t) &((TYPE *)(UI_EXTRADATA_POINTER(0, TYPE)))->MEMBER)

/**
 * @brief Initialize a property from extradata of node
 * @param BEHAVIOUR behaviour Target behaviour
 * @param NAME Name of the property
 * @param TYPE Type of the property
 * @param EXTRADATATYPE Object type containing the property
 * @param ATTRIBUTE Name of the attribute of the object containing data of the property
 */
#define UI_RegisterExtradataNodeProperty(BEHAVIOUR, NAME, TYPE, EXTRADATATYPE, ATTRIBUTE) UI_RegisterNodePropertyPosSize_(BEHAVIOUR, NAME, TYPE, UI_EXTRADATA_OFFSETOF_(EXTRADATATYPE, ATTRIBUTE), MEMBER_SIZEOF(EXTRADATATYPE, ATTRIBUTE))

/**
 * @brief Register a node method to a behaviour.
 * @param behaviour Target behaviour
 * @param name Name of the property
 * @param function function to execute the node method
 * @return A link to the node property
 */
const struct value_s *UI_RegisterNodeMethod(struct uiBehaviour_s *behaviour, const char* name, uiNodeMethod_t function);

/**
 * TODO delete all this shit
 */
#define UI_INIT_PROPERTY(NAME, TYPE, OBJECTTYPE, ATTRIBUTE) {NAME, TYPE, offsetof(OBJECTTYPE, ATTRIBUTE), MEMBER_SIZEOF(OBJECTTYPE, ATTRIBUTE)}
#define UI_INIT_NOSIZE_PROPERTY(NAME, TYPE, OBJECTTYPE, ATTRIBUTE) {NAME, TYPE, offsetof(OBJECTTYPE, ATTRIBUTE), 0}
#define UI_INIT_EXTRADATA_PROPERTY(NAME, TYPE, EXTRADATATYPE, ATTRIBUTE) {NAME, TYPE, UI_EXTRADATA_OFFSETOF_(EXTRADATATYPE, ATTRIBUTE), MEMBER_SIZEOF(EXTRADATATYPE, ATTRIBUTE)}
#define UI_INIT_NOSIZE_EXTRADATA_PROPERTY(NAME, TYPE, EXTRADATATYPE, ATTRIBUTE) {NAME, TYPE, UI_EXTRADATA_OFFSETOF_(EXTRADATATYPE, ATTRIBUTE), 0}
#define UI_INIT_METHOD_PROPERTY(NAME, TYPE, FUNCTION) {NAME, TYPE, ((size_t)FUNCTION), 0}
#define UI_INIT_EMPTY_PROPERTY	{NULL, V_NULL, 0, 0}


#endif
