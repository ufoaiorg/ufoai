--!usr/bin/lua

--[[
-- @file
-- @brief Hospital base menu section
--]]

--[[
Copyright (C) 2002-2019 UFO: Alien Invasion.

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

--[[
 - @todo Add injured employee to the section content (face, healthbar, tooltip)
 - @todo Change hospital screen to change the "map area" only
--]]

do
	require("ufox.lua")
	require("base.section.lua")

	local sectionHhospital = {
		register = function (root_node, base_idx)
			local section = base.build_section(root_node, "hospital", "_Hospital", "icons/hos_bed")
			section:child("header").on_click = function (sender)
				ufo.push_window("hospital", nil, {sender:root():child("base_idx"):as_string(),})
			end
		end,
	}

	return sectionHhospital
end
