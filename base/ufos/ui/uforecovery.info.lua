--!usr/bin/lua

--[[
-- @file
-- @brief UFO Information component for UFO Recovery popup
--]]

--[[
Copyright (C) 2002-2023 UFO: Alien Invasion.

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

require("ufox.lua")

if (uforecovery ~= nil) then
	uforecovery.info = {
		build = function (rootNode, ufoId, ufoModel, ufoDamage)
			if (rootNode:child("ufoInfo") ~= nil) then
				rootNode:child("ufoInfo"):delete_node()
			end
			local ufoInfo = ufox.build({
				name = "ufoInfo",
				class = "panel",
				pos = {0, 0},
				size = {250, 250},
				backgroundcolor = {0.5, 0.5, 0.5, 0.2},

				{
					name = "header",
					class = "string",
					text = "_UFO",
					size = {250, 30},
					contentalign = ufo.ALIGN_CC,
					backgroundcolor = {0.527, 0.6, 0.21, 0.2},
				},

				{
					name = "ufoId",
					class = "string",
					text = ufoId,
					pos = {0, 40},
					size = {250, 20},
					contentalign = ufo.ALIGN_CC,
				},

				{
					name = "ufoModel",
					class = "model",
					model = ufoModel,
					pos = {0, 50},
					size = {250, 200},
					angles = {0, 0, 90},
					autoscale = true,
					omega = {0, 10, 0},
					mouserotate = true,
				},

				{
					name = "ufoDamage",
					class = "string",
					text = string.format("_Damage: %3.0f%%", tonumber(ufoDamage) * 100),
					pos = {0, 230},
					size = {250, 20},
					contentalign = ufo.ALIGN_CC,
				},
			}, rootNode)
		end,
	}
end
