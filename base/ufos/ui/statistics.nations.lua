--!usr/bin/lua

--[[
-- @file
-- @brief Nation statistics widget for statistics screen
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
require("ufox.datatable.lua")

function build_nationstats (rootNode)
	if (rootNode == nil) then
		return
	end

	if (rootNode:child("nationslist") ~= nil) then
		rootNode:child("nationslist"):delete_node()
	end

	local nationStat = ufox.datatable.build(rootNode, {
		name = "nationslist",
		pos = {465, 75},
		size = {545, 235},
	})

	nationStat:add_headerFields({
		{ name = "flag",        width =  20, },
		{ name = "nation_name", width = 270, text = "_Nation", },
		{ name = "funding",     width = 100, text = "_Funding",   contentalign = ufo.ALIGN_CR, },
		{ name = "happiness",   width = 135, text = "_Happiness", contentalign = ufo.ALIGN_CR, },
	})

	local nations_add = ufox.build({
		name = "ui_nations_fill",
		class = "confunc",

		on_click = function (sender, nationID, nationName, monthIDX, nationHappiness, nationHappinessString, nationFunding, nationColor)
			if (monthIDX ~= "0") then
				return
			end

			-- ufo.print(string.format("Got data for nation %s month %s\n", nationID, monthIDX))
			local row = sender:parent():child("nationslist"):add_dataRow({name = nationID}, {
				{ name = "flag",        source = "nations/" .. nationID, class = "image", keepratio = true, ghost = true, },
				{ name = "nation_name", text = nationName, },
				{ name = "funding",     text = nationFunding,            contentalign = ufo.ALIGN_CR, },
				{ name = "happiness",   text = nationHappinessString,    contentalign = ufo.ALIGN_CR, },
			})
		end,
	}, rootNode);

	return nationStat
end
