--!usr/bin/lua

--[[
-- @file
-- @brief Nation charts widget for statistics screen
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
require("ufox.maintabset.lua")

nations = {}
nations.funding = {
	register = function (rootNode, tabset)
		local button = ufox.build({
			name = "funding",
			class = "MainMenuTab",
			text = "_Funding",
			width = 135,

			create = function (sender, nodeName, rootNode)
			end,

			select = function (sender)
				sender:set_backgroundcolor(0.4, 0.515, 0.5, 1)
				sender:set_color(0, 0, 0, 0.9)

				local rootNode = sender:parent():parent()
				local graph = rootNode:child("nation_graph")
				if (graph ~= nil) then
					graph:set_dataid(ufo.LINESTRIP_FUNDING)
				end
			end,

			deselect = function (sender)
				sender:set_backgroundcolor(0.4, 0.515, 0.5, 0.25)
				sender:set_color(1, 1, 1, 0.5)
			end,
		}, tabset)
		return button
	end,
}

nations.happiness = {
	register = function (rootNode, tabset)
		local button = ufox.build({
			name = "happiness",
			class = "MainMenuTab",
			text = "_Happiness",
			width = 135,

			create = function (sender, nodeName, rootNode)
			end,

			select = function (sender)
				sender:set_backgroundcolor(0.4, 0.515, 0.5, 1)
				sender:set_color(0, 0, 0, 0.9)

				local rootNode = sender:parent():parent()
				local graph = rootNode:child("nation_graph")
				if (graph ~= nil) then
					graph:set_dataid(ufo.LINESTRIP_HAPPINESS)
				end
			end,

			deselect = function (sender)
				sender:set_backgroundcolor(0.4, 0.515, 0.5, 0.25)
				sender:set_color(1, 1, 1, 0.5)
			end,
		}, tabset)
		return button
	end,
}

nations.xvi = {
	register = function (rootNode, tabset)
		local button = ufox.build({
			name = "xvi",
			class = "MainMenuTab",
			text = "_XVI Infection",
			width = 135,

			create = function (sender, nodeName, rootNode)
			end,

			select = function (sender)
				sender:set_backgroundcolor(0.4, 0.515, 0.5, 1)
				sender:set_color(0, 0, 0, 0.9)

				local rootNode = sender:parent():parent()
				local graph = rootNode:child("nation_graph")
				if (graph ~= nil) then
					graph:set_dataid(ufo.LINESTRIP_XVI)
				end
			end,

			deselect = function (sender)
				sender:set_backgroundcolor(0.4, 0.515, 0.5, 0.25)
				sender:set_color(1, 1, 1, 0.5)
			end,
		}, tabset)
		return button
	end,
}

function build_nationcharts (rootNode)
	if (rootNode == nil) then
		return
	end

	if (rootNode:child("nation_charts") ~= nil) then
		rootNode:child("nation_charts"):delete_node()
	end
	local nationChart = ufox.build({
		name = "nation_charts",
		class = "panel",
		pos = {0, 0},
		size = {545, 275},
		backgroundcolor = {0.5, 0.5, 0.5, 0.2},

		{
			name = "nation_graph",
			class = "linechart",
			pos = {50, 35},
			size = {492, 200},
			dataid = ufo.LINESTRIP_FUNDING,
			showaxes = true,
			axescolor = {1, 1, 1, 0.5},
		},

		{
			name = "xlabels",
			class = "panel",
			pos = {40, 235},
			size = {502, 20},
			layout = ufo.LAYOUT_LEFT_RIGHT_FLOW,
		},

		{
			name = "xdescription",
			class = "string",
			text = "_Month (Relative to current)",
			pos = {50, 255},
			size = {492, 20},
			contentalign = ufo.ALIGN_CC,
		},
	}, rootNode)

	local xaxis_labels = nationChart:child("xlabels")
	for i = 0, -11, -1 do
		ufox.build({
			name = "label_" .. tostring(i):gsub("-", ""),
			class = "string",
			size = {41, 20},
			text = i,
		}, xaxis_labels)
	end

	local xvi = ufo.getvar("mn_xvimap")
	if (xvi ~= nil and xvi:as_integer() == 1) then
		ufox.build_maintabset(
			nationChart,
			nil,
			nations.funding,
			nations.happiness,
			nations.xvi
		)
	else
	ufox.build_maintabset(
		nationChart,
		nil,
		nations.funding,
		nations.happiness
	)
	end
	nationChart:child("tabset"):select("funding")
	return nationStat
end
