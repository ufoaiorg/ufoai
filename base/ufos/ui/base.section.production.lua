--!usr/bin/lua

--[[
-- @file
-- @brief Production base menu section
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

	local sectionProduction = {
		register = function (root_node, base_idx)
			local section = base.build_section(root_node, "production", "_Production", "icons/wrench")
			section:child("header").on_click = function (sender)
				ufo.push_window("production", nil, nil)
			end

			local add = ufox.build({
				name = "show_production",
				class = "confunc",

				--[[
				 - @brief Show the production item closest to completion in the base overview (not used in default UI)
				 - @param production_idx
				 - @param item_name
				 - @param completition
				--]]
				on_click = function (sender, production_idx, item_name, completition)
					local content_node = sender:parent():child("content")
					local production = ufox.build({
						name = "production_" .. production_idx,
						-- @todo build a progressbar widget eliminate the component
						class = "ProgressBar",

						{ name = "label",    class = "string", text  = "_" .. item_name, },
						{ name = "data_bar", class = "bar",    value = completition},
					}, content_node)

					content_node:set_left(29)
					content_node:update_height()
				end,
			}, section)
			ufo.cmd(string.format("prod_show_active %d;", base_idx + 0))
		end,
	}

	return sectionProduction
end
