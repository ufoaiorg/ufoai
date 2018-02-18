--!usr/bin/lua

--[[
-- @file
-- @brief Soldiers base menu section
--]]

--[[
Copyright (C) 2002-2017 UFO: Alien Invasion.

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
--]]

do
	require("ufox.lua")
	require("base.section.lua")

	local sectionHire = {
		register = function (root_node, base_idx)
			local section = base.build_section(root_node, "hire", "_Soldiers", "icons/crouch")
			section:child("header").on_click = function (sender)
				-- @TODO ufo.push_window() doesn't support parameters
				ufo.cmd("ui_push equipment -1;")
			end
		end,
	}

	return sectionHire
end
