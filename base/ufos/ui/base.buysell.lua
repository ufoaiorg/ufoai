--!usr/bin/lua

--[[
-- @file
-- @brief Market base menu section
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

--header guard
if (base.buysell == nil) then

require("ufox.lua")
require("base.section.lua")

base.buysell = {
	register = function (root_node, base_idx)
		local section = base.build_section(root_node, "buysell", "_Market", "icons/market")
		section:child("header").on_click = function (sender)
			ufo.push_window("market", nil, nil)
		end
	end,
}

--header guard
end
