--!usr/bin/lua

--[[
-- @file
-- @brief Research base menu section
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

	local sectionResearch = {
		register = function (root_node, base_idx)
			local section = base.build_section(root_node, "research", "_Research", "icons/research")
			section:child("header").on_click = function (sender)
				ufo.push_window("research", nil, nil)
			end

			local add = ufox.build({
				name = "show_research",
				class = "confunc",

				--[[
				 - @brief Show a research item in the base overview
				 - @param research_id
				 - @param research_title
				 - @param scientists Number of scientists working on it
				 - @param completition Percent of completition
				--]]
				on_click = function (sender, research_id, research_title, scientists, completition)
					local content_node = sender:parent():child("content")
					local entry = ufox.build({
						name = research_id,
						-- @todo build a progressbar widget eliminate the component
						class = "ProgressBar",

						{ name = "label",    class = "string", text  = "_" .. research_title, },
						{ name = "data_bar", class = "bar",    value = completition},
					}, content_node)

					content_node:set_left(29)
					content_node:update_height()
				end,
			}, section)
			ufo.cmd(string.format("ui_research_show_active %d;", base_idx + 0))
		end,
	}

	return sectionResearch
end
