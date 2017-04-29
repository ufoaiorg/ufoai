--!usr/bin/lua

--[[
-- @file
-- @brief Save campaign main menu tab definition
--]]

--[[
Copyright (C) 2002-2017 UFO: Alien Invasion.

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
			-- disabled = true,

			create = function (sender, nodeName, rootNode)
				local tab = ufox.build({
					name = nodeName,
					class = "panel",
					pos = {0, 35},
					size = {1024, 400},

					{
						name = "savegame_info",
						class = "panel",
						pos = {490, 40},
						size = {534, 360},
						invisible = true,

						{
							name = "filename_label",
							class = "string",
							text = "_Filename:",
							pos = {15, 5},
							size = {180, 20},
							color = {1, 1, 1, 0.5},
						},

						{
							name = "filename",
							class = "textentry",
							text = "*cvar:savegame_filename",
							pos = {205, 5},
							size = {315, 20},
							color = {1, 1, 1, 0.5},
							backgroundcolor = {0, 0, 0, 0.5},
						},

						{
							name = "title_label",
							class = "string",
							text = "_Title:",
							pos = {15, 35},
							size = {180, 20},
							color = {1, 1, 1, 0.5},
						},

						{
							name = "title",
							class = "textentry",
							text = "*cvar:savegame_title",
							pos = {205, 35},
							size = {315, 20},
							color = {1, 1, 1, 0.5},
							backgroundcolor = {0, 0, 0, 0.5},
						},

						{
							name = "gamedate_label",
							class = "string",
							text = "_Game Date:",
							pos = {15, 65},
							size = {180, 20},
							color = {1, 1, 1, 0.5},
						},

						{
							name = "gamedate",
							class = "string",
							text = "",
							pos = {205, 65},
							size = {315, 20},
							color = {1, 1, 1, 0.5},
						},

						{
							name = "savedate_label",
							class = "string",
							text = "_Save Date:",
							pos = {15, 95},
							size = {180, 20},
							color = {1, 1, 1, 0.5},
						},

						{
							name = "savedate",
							class = "string",
							text = "",
							pos = {205, 95},
							size = {315, 20},
							color = {1, 1, 1, 0.5},
						},

						{
							name = "delete",
							class = "MainMenu3Btn",
							text = "_DELETE",
							pos = {284, 250},
							size = {250, 30},
							backgroundcolor = {0.38, 0.48, 0.36, 1},

							on_mouseenter = function (sender)
								sender:set_backgroundcolor(0.59, 0.78, 0.56, 1)
							end,
							on_mouseleave = function (sender)
								sender:set_backgroundcolor(0.38, 0.48, 0.36, 1)
							end,

							on_click = function (sender)
								local tab = sender:parent():parent()
								local filename = ufo.getvar("savegame_filename")
								if (filename == nil) then
									return
								end
								ufo.cmd(string.format("game_delete \"%s\";", filename:as_string()))
								tab:parent():child("tabset"):child(tab:name()):deselect()
								tab:parent():child("tabset"):child(tab:name()):select()
							end
						},
						{
							name = "upload",
							class = "MainMenu3Btn",
							text = "_UPLOAD",
							--tooltip = "_Upload the savegame to the UFO:AI Server",
							disabled = true,
							tooltip = "WebUpload functionality is disabled temporarily",
							pos = {284, 290},
							size = {250, 30},
							backgroundcolor = {0.38, 0.48, 0.36, 1},

							on_mouseenter = function (sender)
								sender:set_backgroundcolor(0.59, 0.78, 0.56, 1)
							end,
							on_mouseleave = function (sender)
								sender:set_backgroundcolor(0.38, 0.48, 0.36, 1)
							end,

							on_click = function (sender)
								local filename = ufo.getvar("savegame_filename")
								if (filename == nil) then
									return
								end
								ufo.cmd(string.format("web_uploadcgame 0 \"%s\";", filename:as_string()))
							end
						},
						{
							name = "save",
							class = "MainMenu3Btn",
							text = "_SAVE",
							pos = {284, 330},
							size = {250, 30},
							backgroundcolor = {0.38, 0.48, 0.36, 1},

							on_mouseenter = function (sender)
								sender:set_backgroundcolor(0.59, 0.78, 0.56, 1)
							end,
							on_mouseleave = function (sender)
								sender:set_backgroundcolor(0.38, 0.48, 0.36, 1)
							end,

							on_click = function (sender)
								local tab = sender:parent():parent()
								local filename = ufo.getvar("savegame_filename")
								if (filename == nil) then
									return
								end
								local title = ufo.getvar("savegame_title")
								if (title == nil) then
									return
								end
								ufo.cmd(string.format("game_save \"%s\" \"%s\";", filename:as_string(), title:as_string()))
								tab:parent():child("tabset"):child(tab:name()):deselect()
								tab:parent():child("tabset"):child(tab:name()):select()
							end
						},
					},
				}, rootNode)
				campaign.listsaves(tab)
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
--					tab:set_invisible(true)
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
