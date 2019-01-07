--!usr/bin/lua

--[[
-- @file
-- @brief Facilities base menu section
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
 - @todo make building preview appear below each option when clicked
 - @todo Make the add building button turn to a warning if it is too high/low (low is for antimatter only)
--]]

do
	require("ufox.lua")
	require("base.section.lua")

	local sectionFacilities = {
		register = function (root_node, base_idx)
			local section = base.build_section(root_node, "facilities", "_Facilities", "icons/facilities")
			section:child("header").on_click = function (sender)
				local content_node = sender:parent():child("content")
				if (content_node:child("building_info") ~= nil) then
					content_node:child("building_info"):delete_node()
				end
				local building = content_node:first()
				while (building ~= nil) do
					if (building:child("hide") == nil) then
						return
					end
					if (building:child("hide"):as_string() == "true") then
						if (building:height() == 0) then
							building:set_height(30)
						else
							building:set_height(0)
						end
					end
					building = building:next()
				end
				content_node:update_height()
			end

			local add = ufox.build({
				name = "show_building",
				class = "confunc",

				--[[
				 - @brief Passes information on capacity for available buildings
				 - @param building_name building name
				 - @param building_id building id (building_lab, building_quarters, building_storage, etc.)
				 - @param cap_one max capacity one building of this type gives
				 - @param cap_current currently used capacity
				 - @param cap_max actual max capacity
				 - @param build_current number of buildings built of this type
				 - @param build_max max number of buildings can be built from this type
				 - @param width Horizontal size of the building in tiles
				 - @param height Vertical size of the building in tiles
				--]]
				on_click = function (sender, building_name, building_id, cap_one, cap_current, cap_max, building_current, building_max, width, height)
					local content_node = sender:parent():child("content")
					local hide = "false"

					if (tonumber(cap_max) == 0 or tonumber(cap_one) <= 1) then
						hide = "true"
					end
					-- Compose the capacity string
					local capacity_string = "0/0"
					if (tonumber(cap_max) > 0) then
						-- Only show the capacity string if max isn't 0
						capacity_string = string.format("%d/%d", cap_current, cap_max)
					elseif (tonumber(building_max) > 0) then
						-- Show current/max number of buildings of a type if limited
						capacity_string = string.format("%d/%d", building_current, building_max)
					end
					-- Build entry
					local building = ufox.build({
						name = building_id,
						class = "BuildingSpace",
						tooltip = building_name,

						{ name = "id",       class = "data",   text = building_id, },
						{ name = "smlicon",  class = "button", icon = "icons/" .. building_id, },
						{ name = "label",    class = "string", text = building_name, },
						{ name = "data",     class = "string", text = capacity_string, },
						{ name = "data_bar", class = "bar",    max = tonumber(cap_max), value = tonumber(cap_current), },
						{ name = "width",    class = "data",   text = width, },
						{ name = "height",   class = "data",   text = height, },
						{ name = "hide",     class = "data",   value = hide, },
						-- Show/Hide "add" building button
						{ name = "alert",    class = "button", invisible = (building_current == building_max), },
					}, content_node)
					if (hide == "true") then
						building:set_height(0)
					end
					content_node:update_height()
				end,
			}, section)
			ufo.cmd(string.format("ui_list_buildings %d;", tonumber(base_idx)))
		end,
	}

	return sectionFacilities
end
