--!usr/bin/lua

--[[
-- @file
-- @brief Campaign success statistics
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

require("ufox.lua")

function build_campaignstats (rootNode)
	if (rootNode == nil) then
		return
	end

	if (rootNode:child("campaign_stats") ~= nil) then
		rootNode:child("campaign_stats"):delete_node()
	end
	local nationChart = ufox.build({
		name = "campaign_stats",
		class = "panel",
		pos = {0, 0},
		size = {545, 150},
		backgroundcolor = {0.5, 0.5, 0.5, 0.2},

		{
			name = "campaign_string",
			class = "string",
			text = "_Campaign",
			pos = {0, 0},
			size = {545, 20},
			contentalign = ufo.ALIGN_CC,
			backgroundcolor = {0.527, 0.6, 0.21, 0.2},
		},

		{
			name = "campaign_stats",
			class = "text",
			dataid = ufo.TEXT_GENERIC,
			pos = {0, 20},
			size = {545, 130},
			lineheight = 22,
			tabwidth = 360,
		},
	}, rootNode)
end
