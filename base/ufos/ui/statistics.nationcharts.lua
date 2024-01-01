--!usr/bin/lua

--[[
-- @file
-- @brief Nation charts widget for statistics screen
--]]

--[[
Copyright (C) 2002-2024 UFO: Alien Invasion.

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
					graph:clear()
					ufo.cmd(string.format("nation_drawcharts funding %s 492 200;", ufo.nodepath(graph)));
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
					graph:clear()
					ufo.cmd(string.format("nation_drawcharts happiness %s 492 200;", ufo.nodepath(graph)));
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
					graph:clear()
					ufo.cmd(string.format("nation_drawcharts xvi %s 492 200;", ufo.nodepath(graph)));
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

		{
			name = "ui_nation_graph_add_line",
			class = "confunc",

			on_click = function (sender, id, visible_str, r, g, b, a, dots_str, num_points)
				local graph = sender:parent():child("nation_graph")
				local visible = false
				if (visible_str == "true") then
					visible = true
				end
				local dots = false
				if (dots_str == "true") then
					dots = true
				end
				graph:add_line(id, visible, r, g, b, a, dots, num_points)
			end,
		},

		{
			name = "ui_nation_graph_add_point",
			class = "confunc",

			on_click = function (sender, id, x, y)
				local graph = sender:parent():child("nation_graph")
				graph:add_point(id, x, y)
			end,
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
