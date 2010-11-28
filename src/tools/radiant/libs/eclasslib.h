/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined (INCLUDED_ECLASSLIB_H)
#define INCLUDED_ECLASSLIB_H

#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <map>
#include <vector>

#include "ieclass.h"
#include "irender.h"

#include "string/string.h"

typedef Vector3 Colour3;

class ListAttributeType
{
		typedef std::pair<std::string, std::string> ListItem;
		typedef std::vector<ListItem> ListItems;
		ListItems m_items;
	public:

		typedef ListItems::const_iterator const_iterator;
		const_iterator begin () const
		{
			return m_items.begin();
		}
		const_iterator end () const
		{
			return m_items.end();
		}

		const ListItem& operator[] (std::size_t i) const
		{
			return m_items[i];
		}
		const_iterator findValue (const std::string& value) const
		{
			for (ListItems::const_iterator i = m_items.begin(); i != m_items.end(); ++i) {
				if (value == i->second)
					return i;
			}
			return m_items.end();
		}

		void push_back (const std::string& name, const std::string& value)
		{
			m_items.push_back(ListItems::value_type(name, value));
		}
};

class EntityClassAttribute
{
	public:
		std::string type; /**< type used as entity key and to decide how gui representation will look like @sa EntityAttributeFactory */
		std::string name; /**< name used for display in entityinspector */
		std::string value; /**< current attribute value */
		std::string description; /**< actually not used, could be used as a tooltip @todo use this as tooltip in entityinspector?*/
		bool mandatory; /**< if this is true, the value is needed for the entity to work */
		EntityClassAttribute ()
		{
		}
		EntityClassAttribute (const std::string& type, const std::string& name, bool mandatory = false, const std::string& value = "",
				const std::string& description = "") :
			type(type), name(name), value(value), description(description), mandatory(mandatory)
		{
		}

		std::string getNullValue () const
		{
			if (type == "spawnflags") {
				return "0";
			} else if (type == "float") {
				return "0.0";
			}
			return "-";
		}
};

typedef std::pair<std::string, EntityClassAttribute> EntityClassAttributePair;
typedef std::list<EntityClassAttributePair> EntityClassAttributes;
typedef std::list<std::string> StringList;

/**
 * Visitor class for EntityClassAttributes.
 *
 * \ingroup eclass
 */
struct EntityClassAttributeVisitor {
	virtual ~EntityClassAttributeVisitor() {}

	/**
	 * Visit function.
	 *
	 * @param attr
	 * The current EntityClassAttribute to visit.
	 */
	virtual void visit(const EntityClassAttribute&) = 0;
};

class EntityClass
{
	public:
		std::string m_name;
		StringList m_parent;
		bool fixedsize;
		Vector3 mins;
		Vector3 maxs;

		Colour3 color;
		Shader* m_state_fill;
		Shader* m_state_wire;
		Shader* m_state_blend;

		std::string m_comments;
		typedef std::vector<std::string> FlagNames;
		FlagNames flagnames;

		std::string m_modelpath; /** model path - only for displaying in radiant */
		std::string m_skin;

		void (*free) (EntityClass*);

		EntityClassAttributes m_attributes;

		const std::string& name () const
		{
			return m_name;
		}
		const std::string& comments () const
		{
			return m_comments;
		}
		const std::string& modelpath () const
		{
			return m_modelpath;
		}
		const std::string& skin () const
		{
			return m_skin;
		}

		/**
		 * Enumerate the EntityClassAttibutes in turn.
		 *
		 * @param visitor
		 * An EntityClassAttributeVisitor instance.
		 */
		void forEachClassAttribute (EntityClassAttributeVisitor& visitor) const
		{
			for (EntityClassAttributes::const_iterator i = m_attributes.begin(); i != m_attributes.end(); ++i) {
				// Visit if it is a non-editor key or we are visiting all keys
				visitor.visit(i->second);
			}
		}

		/**
		 * @brief Checks whether am attribute is mandatory for the entity.
		 *
		 * @note classname is always mandatory
		 */
		bool isMandatory (const std::string& attributeName) const
		{
			if (attributeName == "classname")
				return true;
			for (EntityClassAttributes::const_iterator i = m_attributes.begin(); i != m_attributes.end(); ++i) {
				if (attributeName == i->first) {
					const EntityClassAttribute& attr = i->second;
					return attr.mandatory;
				}
			}
			return false;
		}

		/**
		 * @brief Get the attribute definition for a given attribute name
		 * @param attributeName the attribute to retrieve
		 * @return attribute or @c NULL
		 */
		EntityClassAttribute *getAttribute (const std::string& attributeName) const
		{
			for (EntityClassAttributes::const_iterator i = m_attributes.begin(); i != m_attributes.end(); ++i) {
				if (attributeName == i->first) {
					return const_cast<EntityClassAttribute*> (&(*i).second);
				}
			}
			return NULL;
		}

		/**
		 * @brief Get the default value for a given entity parameter
		 * @param attributeName The attribute name to get the default value for
		 * @return the default value
		 */
		const std::string getDefaultForAttribute (const std::string& attributeName) const
		{
			EntityClassAttribute *attrib = getAttribute(attributeName);
			//use value if it is set to something
			if (attrib && attrib->value.length() > 0)
				return attrib->value;
			else if (attrib)
				return attrib->getNullValue();
			return "-";
		}
};

inline std::string EntityClass_valueForKey (const EntityClass& entityClass, const std::string& key)
{
	for (EntityClassAttributes::const_iterator i = entityClass.m_attributes.begin(); i
			!= entityClass.m_attributes.end(); ++i) {
		if (key == (*i).first) {
			return (*i).second.value;
		}
	}
	return "";
}

inline EntityClassAttributePair& EntityClass_insertAttribute (EntityClass& entityClass, const std::string& key,
		const EntityClassAttribute& attribute = EntityClassAttribute())
{
	entityClass.m_attributes.push_back(EntityClassAttributePair(key, attribute));
	return entityClass.m_attributes.back();
}

inline void buffer_write_colour_fill (char buffer[128], const Colour3& colour)
{
	sprintf(buffer, "(%g %g %g)", colour[0], colour[1], colour[2]);
}

inline void buffer_write_colour_wire (char buffer[128], const Colour3& colour)
{
	sprintf(buffer, "<%g %g %g>", colour[0], colour[1], colour[2]);
}

inline void buffer_write_colour_blend (char buffer[128], const Colour3& colour)
{
	sprintf(buffer, "[%g %g %g]", colour[0], colour[1], colour[2]);
}

inline Shader* colour_capture_state_fill (const Colour3& colour)
{
	char buffer[128];
	buffer_write_colour_fill(buffer, colour);
	return GlobalShaderCache().capture(buffer);
}

inline void colour_release_state_fill (const Colour3& colour)
{
	char buffer[128];
	buffer_write_colour_fill(buffer, colour);
	GlobalShaderCache().release(buffer);
}

inline Shader* colour_capture_state_wire (const Colour3& colour)
{
	char buffer[128];
	buffer_write_colour_wire(buffer, colour);
	return GlobalShaderCache().capture(buffer);
}

inline void colour_release_state_wire (const Colour3& colour)
{
	char buffer[128];
	buffer_write_colour_wire(buffer, colour);
	GlobalShaderCache().release(buffer);
}

inline Shader* colour_capture_state_blend (const Colour3& colour)
{
	char buffer[128];
	buffer_write_colour_blend(buffer, colour);
	return GlobalShaderCache().capture(buffer);
}

inline void colour_release_state_blend (const Colour3& colour)
{
	char buffer[128];
	buffer_write_colour_blend(buffer, colour);
	GlobalShaderCache().release(buffer);
}

inline void eclass_capture_state (EntityClass* eclass)
{
	eclass->m_state_fill = colour_capture_state_fill(eclass->color);
	eclass->m_state_wire = colour_capture_state_wire(eclass->color);
	eclass->m_state_blend = colour_capture_state_blend(eclass->color);
}

inline void eclass_release_state (EntityClass* eclass)
{
	colour_release_state_fill(eclass->color);
	colour_release_state_wire(eclass->color);
	colour_release_state_blend(eclass->color);
}

// eclass constructor
inline EntityClass* Eclass_Alloc ()
{
	EntityClass* e = new EntityClass;

	e->fixedsize = false;

	for (int i = 0; i < 32; i++)
		e->flagnames.push_back("");

	e->maxs = Vector3(-1, -1, -1);
	e->mins = Vector3(1, 1, 1);
	e->color = Vector3(1, 1, 1);

	e->free = 0;

	return e;
}

// eclass destructor
inline void Eclass_Free (EntityClass* e)
{
	eclass_release_state(e);

	delete e;
}

inline EntityClass* EClass_Create (const std::string& name, const Vector3& colour, const std::string& comments)
{
	EntityClass *e = Eclass_Alloc();
	e->free = &Eclass_Free;

	e->m_name = name;

	e->color = colour;
	eclass_capture_state(e);

	e->m_comments = comments;

	return e;
}

inline EntityClass* EClass_Create_FixedSize (const std::string& name, const Vector3& colour, const Vector3& mins,
		const Vector3& maxs, const std::string& comments)
{
	EntityClass *e = Eclass_Alloc();
	e->free = &Eclass_Free;

	e->m_name = name;

	e->color = colour;
	eclass_capture_state(e);

	e->fixedsize = true;

	e->mins = mins;
	e->maxs = maxs;

	e->m_comments = comments;

	return e;
}

const Vector3 smallbox[2] = { Vector3(-8, -8, -8), Vector3(8, 8, 8), };

inline EntityClass *EntityClass_Create_Default (const std::string& name, bool has_brushes)
{
	// create a new class for it
	if (has_brushes) {
		return EClass_Create(name, Vector3(0.0f, 0.5f, 0.0f), "Not found in source.");
	} else {
		return EClass_Create_FixedSize(name, Vector3(0.0f, 0.5f, 0.0f), smallbox[0], smallbox[1],
				"Not found in source.");
	}
}

#endif
