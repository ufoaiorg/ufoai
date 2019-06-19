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

function build_nationstats (rootNode)
	if (rootNode == nil) then
		return
	end

	if (rootNode:child("nation_stats") ~= nil) then
		rootNode:child("nation_stats"):delete_node()
	end
	local nationStat = ufox.build({
		name = "nation_stats",
		class = "panel",
		pos = {0, 0},
		size = {545, 235},
		backgroundcolor = {0.5, 0.5, 0.5, 0.2},

		{
			name = "header",
			class = "panel",
			pos = {0, 0},
			size = {545, 20},
			layout = ufo.LAYOUT_LEFT_RIGHT_FLOW,
			backgroundcolor = {0.527, 0.6, 0.21, 0.2},

			{
				name = "placeholder",
				class = "panel",
				size = {20, 20},
			},

			{
				name = "nation_name",
				class = "string",
				text = "_Nation",
				size = {270, 20},
				contentalign = ufo.ALIGN_CC,
			},

			{
				name = "nation_funding_label",
				class = "string",
				text = "_Funding",
				size = {100, 20},
				contentalign = ufo.ALIGN_CR,
			},

			{
				name = "nation_happiness_label",
				class = "string",
				text = "_Happiness",
				size = {135, 20},
				contentalign = ufo.ALIGN_CR,
			},
		},

		{
			name = "nation_list",
			class = "panel",
			pos = {0, 20},
			size = {525, 215},
			layout = ufo.LAYOUT_TOP_DOWN_FLOW,
			wheelscrollable = true,

			on_viewchange = function (sender)
				local scrollbar = sender:parent():child("nation_list_scrollbar")
				scrollbar:set_fullsize(sender:fullsize())
				scrollbar:set_current(sender:viewpos())
				scrollbar:set_viewsize(sender:viewsize())
			end,

			on_wheel = function (sender)
				local scrollbar = sender:parent():child("nation_list_scrollbar")
				scrollbar:set_current(sender:viewpos())
			end,

			clear = function (sender)
				sender:remove_children()
			end,

			add = function (sender, nationID, nationName, nationFlag, nationFunding, nationHappinessString)
				local row = ufox.build({
					name = nationID,
					class = "panel",
					size = {520, 20},
					layout = ufo.LAYOUT_LEFT_RIGHT_FLOW,

					{
						name = "nation_enabled",
						class = "image",
						size = {20, 20},
						--backgroundcolor = {0.5, 1, 1, 1},
						source = nationFlag,
						keepratio = true;
						ghost = true,
					},

					{
						name = "nation_name",
						class = "string",
						text = nationName,
						size = {270, 20},
						ghost = true,
					},

					{
						name = "nation_funding",
						class = "string",
						text = string.format("%s c", nationFunding),
						contentalign = ufo.ALIGN_CR,
						size = {100, 20},
						ghost = true,
					},

					{
						name = "nation_happiness",
						class = "string",
						text = nationHappinessString,
						contentalign = ufo.ALIGN_CR,
						size = {135, 20},
						ghost = true,
					},
				}, sender)
			end,
		},

		{
			name = "nation_list_scrollbar",
			class = "vscrollbar",
			image = "ui/scrollbar_v",
			pos = {525, 20},
			height = 215,
			current = 0,
			viewsize = 10,
			fullsize = 0,
			autoshowscroll = true,

			on_change = function (sender)
				local panel = sender:parent():child("nation_list")
				panel:set_viewpos(sender:current())
			end,
		},

		{
			name = "ui_nations_clear",
			class = "confunc",

			on_click = function (sender)
				sender:parent():child("nation_list"):clear()
			end,
		},

		{
			name = "ui_nations_fill",
			class = "confunc",

			on_click = function (sender, nationID, nationName, monthIDX, nationHappiness, nationHappinessString, nationFunding, nationColor)
				if (monthIDX ~= "0") then
					return
				end

				-- ufo.print(string.format("Got data for nation %s month %s\n", nationID, monthIDX))
				sender:parent():child("nation_list"):add(
					nationID,
					nationName,
					"nations/" .. nationID,
					nationFunding,
					nationHappinessString
				)
			end,
		},
	}, rootNode)

	return nationStat
end
