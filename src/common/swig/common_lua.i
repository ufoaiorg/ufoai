/**
	@file common_lua.i
	@brief Interface file for SWIG to generarte lua ui binding.

	SWIG interface file for the lua ufo module. Common ufo functions are grouped in lua using
	the ufo namespace. Usage in lua is like this:

    @code
    function my_lua_function ()
		ufo.printf ("hello from the lua script")
		ufo.error (1, "oops, something is not right!")
    end
    @endcode
*/

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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

/* expose common ufo stuff */
%module ufo

%{
/* import headers into the interface so they can be used */
#include <typeinfo>

/* import common functions */
#include "../../shared/shared.h"
%}

%rename (print) Com_Printf;
void Com_Printf (const char* fmt);
%rename (dprint) Com_DPrintf;
void Com_DPrintf(int level, const char* fmt);
%rename (error) Com_Error;
void Com_Error(int code, const char* fmt);
