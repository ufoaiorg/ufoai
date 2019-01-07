--!usr/bin/lua

--[[
-- @file
-- @brief Build base header
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

-- header guard
if (base.build_header == nil) then

base.build_header = function (root_node, base_idx)
	local header = root_node:child("base_header")
	if (header ~= nil) then
		header:delete_node()
	end
	header = ufox.build({
		name = "base_header",
		class = "panel",
		pos = {80, 5},
		size = {640, 32},

		{
			name = "base_name",
			class = "textentry",
			-- @todo textentry should work without a cvar
			text = "*cvar:mn_base_title",
			pos = {0, 0},
			size = {360, 32},
			font = "f_small_bold",
			color = {0.56, 0.81, 0.76, 1},
			contentalign = ufo.ALIGN_CL,

			on_change = function (sender)
				local base_idx = sender:root():child("base_idx")
				if (base_idx == nil) then
					return
				end
				local base_name = ufo.getvar("mn_base_title")
				if (base_name == nil) then
					return
				end
				ufo.cmd(string.format("base_changename %s %q;", base_idx:as_string(), base_name:as_string()))
			end,
		},

		{
			name = "base_selector",
			class = "panel",
			pos = {365, 7},
			size = {160, 16},
			layout = ufo.LAYOUT_COLUMN,
			layoutcolumns = 8,
			layoutmargin = 4,
		},

		{
			name = "credits",
			class = "string",
			-- @todo do not use cvar
			text = "*cvar:mn_credits",
			pos = {525, 0},
			size = {120, 32},
			font = "f_small_bold",
			color = {0.56, 0.81, 0.76, 1},
			contentalign = ufo.ALIGN_CR,
		},
	}, root_node)

	for i = 0, 7 do
		local mini = ufox.build({
			name = "base_" .. i,
			class = "BaseLayoutMini",
			baseid = i,
		}, header:child("base_selector"))
	end
end

-- header guard
end