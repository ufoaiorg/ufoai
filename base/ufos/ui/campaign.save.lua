--!usr/bin/lua

--[[
-- @file
-- @brief Save campaign main menu tab definition
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

-- campaign.save Header Guard
if (campaign.save == nil) then

require("ufox.lua")
require("campaign.listsaves.lua")

campaign.save = {
	register = function (rootNode, tabset)
		local button = ufox.build({
			name = "save",
			class = "MainMenuTab",
			text = "_SAVE",

			select = function (sender)
				sender:set_backgroundcolor(0.4, 0.515, 0.5, 1)
				sender:set_color(0, 0, 0, 0.9)

				local rootNode = sender:parent():parent()
				local tab = rootNode:child(sender:name())
				if (tab ~= nil) then
					sender:deselect()
				end

				tab = ufox.build({
					name = sender:name(),
					class = "panel",
					pos = {0, 35},
					size = {1024, 400},
				}, rootNode)
				campaign.listsaves(tab)

				-- (re-)load savegames
				ufo.cmd(
					"ui_clear_savegames;" ..
					"ui_add_savegame new \"\" \"\" \"\" \"\";" ..
					"game_listsaves;"
				)
			end,

			deselect = function (sender)
				sender:set_backgroundcolor(0.4, 0.515, 0.5, 0.25)
				sender:set_color(1, 1, 1, 0.5)

				local rootNode = sender:parent():parent()
				local tab = rootNode:child(sender:name())
				if (tab ~= nil) then
					tab:remove_children()
					tab:delete_node()
				end
			end,
		}, tabset)

		return button
	end,
}

-- campaign.save Header Guard
end
