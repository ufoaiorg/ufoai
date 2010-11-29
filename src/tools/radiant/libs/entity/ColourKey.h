/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

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

#if !defined(INCLUDED_COLOUR_H)
#define INCLUDED_COLOUR_H

#include "ientity.h"
#include "irender.h"

#include "eclasslib.h"
#include "generic/callback.h"
#include "stringio.h"

class ColourKey
{
	private:

		void default_colour ()
		{
			m_colour = Vector3(1, 1, 1);
		}

		Callback m_colourChanged;
		Shader* m_state;

		void capture_state ()
		{
			m_state = colour_capture_state_fill(m_colour);
		}
		void release_state ()
		{
			colour_release_state_fill(m_colour);
		}

	public:
		Vector3 m_colour;

		ColourKey (const Callback& colourChanged) :
			m_colourChanged(colourChanged)
		{
			default_colour();
			capture_state();
		}
		~ColourKey ()
		{
			release_state();
		}

		void colourChanged (const std::string& value)
		{
			release_state();
			if (!string_parse_vector3(value, m_colour)) {
				default_colour();
			}
			capture_state();

			m_colourChanged();
		}
		typedef MemberCaller1<ColourKey, const std::string&, &ColourKey::colourChanged> ColourChangedCaller;

		void write (Entity* entity) const
		{
			entity->setKeyValue("_color", m_colour);
		}

		Shader* state () const
		{
			return m_state;
		}
};

#endif
