--!usr/bin/lua

--[[
-- @file
-- @brief Employees base menu section
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

	local sectionEmployees = {
		register = function (root_node, base_idx)
			local section = base.build_section(root_node, "employees", "_Employees", "icons/crouch")
			section:child("header").on_click = function (sender)
				ufo.push_window("employees", nil, {0})
			end

			local add = section:child("show_employee_counts")
			if (add ~= nil) then
				add:delete_node()
			end
			add = ufox.build({
				name = "show_employee_counts",
				class = "confunc",

				on_click = function (sender, employeeType, count, employeeName)
					local content_node = sender:parent():child("content")
					content_node:set_layout(ufo.LAYOUT_LEFT_RIGHT_FLOW)
					local empl = ufox.build({
						name = employeeType,
						class = "panel",
						size = {74, 48},
						tooltip = employeeName,

						{
							name = "head",
							class = "button",
							icon = "icons/head_" .. employeeType,
							pos = {0, 0},
							size = {48, 48},
							ghost = true,
						},

						{
							name = "count",
							class = "string",
							text = count,
							pos = {36, 0},
							size = {36, 24},
							color = {0.56, 0.81, 0.76, 1},
							contentalign = ufo.ALIGN_CC,
							ghost = true,
						},

						on_click = function (sender)
							-- @todo remove this magic number
							local employeeTypeNum = 0
							if (employeeType == "soldier") then
								employeeTypeNum = 0
							elseif (employeeType == "scientist") then
								employeeTypeNum = 1
							elseif (employeeType == "worker") then
								employeeTypeNum = 2
							elseif (employeeType == "pilot") then
								employeeTypeNum = 3
							end
							ufo.push_window("employees", nil, {employeeTypeNum})
						end,
					}, content_node)

					if (employeeType == "soldier" and tonumber(count) > 0) then
						ufox.build({
							name = "weapon",
							class = "button",
							icon = "icons/icon_primarysml",
							tooltip = "_Equip soldiers",
							pos = {36, 24},
							size = {36, 24},

							on_click = function (sender)
								ufo.push_window("equipment", nil, {-1})
							end,
						}, empl)
					end

					content_node:set_left(8)
					content_node:update_height()
				end,
			}, section)
			ufo.cmd(string.format("ui_get_employee_counts show_employee_counts %d;", tonumber(base_idx)))
		end,
	}

	return sectionEmployees
end
