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

#if !defined (INCLUDED_SHADERLIB_H)
#define INCLUDED_SHADERLIB_H

#include "string/string.h"
#include "character.h"
#include "ishadersystem.h"

//! @deprecated - use string compare directory. Shaders are only equal if filenames and shader ids are matching
inline bool shader_equal (const std::string& shader, const std::string& other)
{
	return string_equal_nocase(shader, other);
}

inline bool shader_equal_n (const std::string& shader, const std::string& other, std::size_t n)
{
	return string_equal_nocase_n(shader, other, n);
}

inline bool shader_less (const std::string& shader, const std::string& other)
{
	return string_less_nocase(shader, other);
}

inline bool shader_equal_prefix (const std::string& string, const std::string& prefix)
{
	return shader_equal_n(string, prefix, prefix.length());
}

class shader_less_t
{
	public:
		bool operator() (const std::string& shader, const std::string& other) const
		{
			return shader_less(shader, other);
		}
};

inline bool shader_valid (const std::string& shader)
{
	return string_is_ascii(shader) && strchr(shader.c_str(), ' ') == 0 && strchr(shader.c_str(), '\n') == 0 && strchr(shader.c_str(), '\r')
			== 0 && strchr(shader.c_str(), '\t') == 0 && strchr(shader.c_str(), '\v') == 0 && strchr(shader.c_str(), '\\') == 0;
}

inline const std::string& GlobalTexturePrefix_get ()
{
	return GlobalShaderSystem().getTexturePrefix();
}

inline bool shader_is_texture (const std::string& name)
{
	return shader_equal_prefix(name, GlobalTexturePrefix_get());
}

inline std::string shader_get_textureName (const std::string& name)
{
	return name.substr(GlobalTexturePrefix_get().length());
}

inline bool texdef_name_valid (const std::string& name)
{
	return shader_valid(name) && shader_is_texture(name);
}

#endif
