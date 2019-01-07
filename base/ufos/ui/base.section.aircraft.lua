--!usr/bin/lua

--[[
-- @file
-- @brief Aircraft base menu section
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

do
	require("ufox.lua")
	require("base.section.lua")

	local sectionAircraft = {
		register = function (root_node, base_idx)
			local section = base.build_section(root_node, "aircraft", "_Aircraft", "icons/aircraft")
			section:child("header").on_click = function (sender)
				ufo.push_window("aircraft_equip", nil, nil)
			end

			local add = ufox.build({
				name = "show_aircraft",
				class = "confunc",

				--[[
				 - @brief Show the base's aircraft
				 - @param aircraft_idx id global aircraft idx
				 - @param aircraft_name Name of the aircraft
				 - @param aircraft_type Aircraft type
				 - @param aircraft_status aircraft status
				 - @param aircraft_in_base if the aircraft is in the base
				 - @param idx_in_base Numeric id of the aircraft in the base (for aircraft_equip menu)
				--]]
				on_click = function (sender, aircraft_idx, aircraft_name, aircraft_type, aircraft_status, aircraft_in_base, idx_in_base)
					local aircraft_icon = "icons/" .. aircraft_type
					if (aircraft_in_base ~= "1") then
						aircraft_icon = string.format("%s_off", aircraft_icon)
					end

					local content_node = sender:parent():child("content")
					content_node:set_layout(ufo.LAYOUT_LEFT_RIGHT_FLOW)
					local entry = ufox.build({
						name = "aircraft_" .. aircraft_idx,
						-- @todo build a progressbar widget eliminate the component
						class = "panel",
						size = {48, 48},

						{
							name = "idx",
							class = "data",
							text = idx_in_base,
						},

						{
							name = "icon",
							class = "button",
							pos = {0, 0},
							size = {48, 48},
							icon = aircraft_icon,
							tooltip = string.format("%s (%s)", aircraft_name, aircraft_status),

							on_click = function (sender)
								-- @TODO ufo.push_window() doesn't support parameters
								ufo.cmd(string.format("ui_push aircraft_equip %q;", sender:parent():child("idx"):as_string()))
							end,
						},
					}, content_node)

					content_node:set_left(8)
					content_node:update_height()
				end,
			}, section)
			ufo.cmd(string.format("ui_show_aircraft %d;", tonumber(base_idx)))
		end,
	}

	return sectionAircraft
end
