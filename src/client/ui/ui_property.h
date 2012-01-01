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

/**
 * @brief Return the offset of an extradata node attribute
 * @param TYPE Extradata type
 * @param MEMBER Attribute name
 * @sa offsetof
 */
#define UI_EXTRADATA_OFFSETOF(TYPE, MEMBER) ((size_t) &((TYPE *)(UI_EXTRADATA_POINTER(0, TYPE)))->MEMBER)

/**
 * @brief Initialize a property
 * @param NAME Name of the property
 * @param TYPE Type of the property
 * @param OBJECTTYPE Object type containing the property
 * @param ATTRIBUTE Name of the attribute of the object containing data of the property
 */
#define UI_INIT_PROPERTY(NAME, TYPE, OBJECTTYPE, ATTRIBUTE) {NAME, TYPE, offsetof(OBJECTTYPE, ATTRIBUTE), MEMBER_SIZEOF(OBJECTTYPE, ATTRIBUTE)}
#define UI_INIT_NOSIZE_PROPERTY(NAME, TYPE, OBJECTTYPE, ATTRIBUTE) {NAME, TYPE, offsetof(OBJECTTYPE, ATTRIBUTE), 0}

/**
 * @brief Initialize a property from extradata of node
 * @param NAME Name of the property
 * @param TYPE Type of the property
 * @param EXTRADATATYPE Object type containing the property
 * @param ATTRIBUTE Name of the attribute of the object containing data of the property
 */
#define UI_INIT_EXTRADATA_PROPERTY(NAME, TYPE, EXTRADATATYPE, ATTRIBUTE) {NAME, TYPE, UI_EXTRADATA_OFFSETOF(EXTRADATATYPE, ATTRIBUTE), MEMBER_SIZEOF(EXTRADATATYPE, ATTRIBUTE)}
#define UI_INIT_NOSIZE_EXTRADATA_PROPERTY(NAME, TYPE, EXTRADATATYPE, ATTRIBUTE) {NAME, TYPE, UI_EXTRADATA_OFFSETOF(EXTRADATATYPE, ATTRIBUTE), 0}

/**
 * @brief Initialize a property which is a method to an object
 * @param NAME Name of the property
 * @param TYPE Type of the property
 * @param FUNCTION Reference to the function which is used like a method
 * @param ATTRIBUTE Name of the attribute of the object containing data of the property
 */
#define UI_INIT_METHOD_PROPERTY(NAME, TYPE, FUNCTION) {NAME, TYPE, ((size_t)FUNCTION), 0}

/**
 * Init empty property used to finish a property list
 */
#define UI_INIT_EMPTY_PROPERTY	{NULL, V_NULL, 0, 0}

#endif
