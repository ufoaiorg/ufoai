--!usr/bin/lua

--[[
-- @file
-- @brief Multiplayer game Team management tab definition
--]]

--[[
Copyright (C) 2002-2023 UFO: Alien Invasion.

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

if (multiplayer.team == nil) then
	require("ufox.lua")
	multiplayer.team = {
		register = function (rootNode, tabset)
			local button = ufox.build({
				name = "team",
				class = "MainMenuTab",
				text = "_TEAM",
				width = 135,

				create = function (sender, nodeName, rootNode)
					local tab = ufox.build({
						name = nodeName,
						class = "panel",
						pos = {0, 35},
						size = {1024, 400},

						{
							name = "Title",
							class = "string",
							text = "TEAM",
						},
					}, rootNode)
				end,

				select = function (sender)
					sender:set_backgroundcolor(0.4, 0.515, 0.5, 1)
					sender:set_color(0, 0, 0, 0.9)

					local rootNode = sender:parent():parent()
					local tab = rootNode:child(sender:name())
					if (tab ~= nil) then
						tab:set_invisible(false)
					else
						tab = sender:create(sender:name(), rootNode)
					end
				end,

				deselect = function (sender)
					sender:set_backgroundcolor(0.4, 0.515, 0.5, 0.25)
					sender:set_color(1, 1, 1, 0.5)

					local rootNode = sender:parent():parent()
					local tab = rootNode:child(sender:name())
					if (tab ~= nil) then
						tab:set_invisible(true)
					end
				end,
			}, tabset)
			return button
		end,
	}
end
