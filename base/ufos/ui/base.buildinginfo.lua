--!usr/bin/lua

--[[
-- @file
-- @brief Build the building information panel
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

--header guard
if (base.build_buildinginfo == nil) then

require("ufox.lua")
require("base.section.lua")

base.build_buildinginfo = function (root_node, base_idx, building_id)
	local building_info = root_node:child("building_info")
	if (building_info ~= nil) then
		-- delete (and later re-build) section
		building_info:delete_node()
	end
	building_info = ufox.build({
		name = "building_info",
		class = "panel",
		size = {root_node:width(), 200},

		{
			name = "picture",
			class = "image",
			source = "",
			keepratio = true,
			pos = {0, 0},
			size = {root_node:width(), 90},
			contentalign = ufo.ALIGN_CC,
			ghost = true,
		},

		{
			name = "building_name",
			class = "string",
			pos = {5, 100},
			size = {root_node:width(), 20},
			text = "",
			contentalign = ufo.ALIGN_CC,
		},

		{
			name = "cost_label",
			class = "string",
			pos = {5, 120},
			size = {150, 20},
			text = "_Costs",
			contentalign = ufo.ALIGN_CL,
		},
		{
			name = "cost_string",
			class = "string",
			pos = {155, 120},
			size = {134, 20},
			text = "",
			contentalign = ufo.ALIGN_CR,
		},

		{
			name = "runningcost_label",
			class = "string",
			pos = {5, 140},
			size = {150, 20},
			text = "_Running costs",
			contentalign = ufo.ALIGN_CL,
		},
		{
			name = "runningcost_string",
			class = "string",
			pos = {155, 140},
			size = {134, 20},
			text = "",
			contentalign = ufo.ALIGN_CR,
		},

		{
			name = "buildtime_label",
			class = "string",
			pos = {5, 160},
			size = {150, 20},
			text = "_Build time",
			contentalign = ufo.ALIGN_CL,
		},
		{
			name = "buildtime_string",
			class = "string",
			pos = {155, 160},
			size = {134, 20},
			text = "",
			contentalign = ufo.ALIGN_CR,
		},

		{
			name = "depends_label",
			class = "string",
			pos = {5, 180},
			size = {150, 20},
			text = "_Depends on",
			contentalign = ufo.ALIGN_CL,
		},
		{
			name = "depends_string",
			class = "string",
			pos = {155, 180},
			size = {134, 20},
			text = "",
			contentalign = ufo.ALIGN_CR,
		},

		{
			name = "show_buildinginfo",
			class = "confunc",

			--[[
			 - @brief Passes building information
			 - @param building_id scripted building id
			 - @param building_name building name
			 - @param building_image
			 - @param fixcost cost to built this building
			 - @param runningcost monthly maintenance cost of the building
			 - @param buildtime number of days building this building takes
			 - @param buildtime_string translated string of buildtime
			 - @param depends name of the building this one needs in order to work
			--]]
			on_click = function (sender, building_id, building_name, building_image, fixcost, runningcost, buildtime, buildtime_string, depends)
				local panel = sender:parent()
				panel:child("picture"):set_source(building_image)
				panel:child("building_name"):set_text(string.format("_%s", building_name))
				if (tonumber(fixcost) > 0) then
					panel:child("cost_string"):set_text(string.format("%d c", fixcost))
				else
					panel:child("cost_label"):delete_node()
					panel:child("cost_string"):delete_node()
				end
				if (tonumber(runningcost) > 0) then
					panel:child("runningcost_string"):set_text(string.format("%d c", runningcost))
				else
					panel:child("runningcost_label"):delete_node()
					panel:child("runningcost_string"):delete_node()
				end
				if (tonumber(buildtime) > 0) then
					panel:child("buildtime_string"):set_text(buildtime_string)
				else
					panel:child("buildtime_label"):delete_node()
					panel:child("buildtime_string"):delete_node()
				end
				if (depends ~= nil and depends ~= "") then
					panel:child("depends_string"):set_text(string.format("_%s", depends))
				else
					panel:child("depends_label"):delete_node()
					panel:child("depends_string"):delete_node()
				end
			end,
		},
	}, root_node)
	if (root_node.update_height ~= nil) then
		root_node:update_height()
	end
	ufo.cmd(string.format("ui_show_buildinginfo %d %q;", tonumber(base_idx), building_id))
end

--header guard
end
