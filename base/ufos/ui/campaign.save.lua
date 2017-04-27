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
						name = "selected_savegame",
						class = "data",
						text = ""
					},

					{
						name = "savegame_list",
						class = "panel",
						pos = {10, 40},
						size = {450, 320},
						layout = ufo.LAYOUT_TOP_DOWN_FLOW,
						layoutmargin = 5,
						wheelscrollable = true,
						backgroundcolor = {0, 0, 0, 0.5},

						clear = function (sender)
							sender:remove_children()
						end,

						add = function (sender, idx, title, gamedate, realdate, filename)
							if (idx == nil) then
								return
							end

							local node = ufox.build({
								name = "slot_" .. idx,
								class = "panel",
								size = {450, 35},
								tooltip = "_Saved at: " .. realdate,

								on_click = function (sender)
									sender:parent():select(sender:name())
								end,

								{
									name = "id",
									class = "data",
									text = filename,
								},

								{
									name = "title",
									class = "string",
									pos = {0, 0},
									size = {450, 20},
									text = title,
									color = {1, 1, 1, 0.5},
									ghost = true,
								},

								{
									name = "gamedate",
									class = "string",
									pos = {0, 25},
									size = {450, 10},
									text = gamedate,
									color = {1, 1, 1, 0.5},
									font = "f_verysmall",
									contentalign = ufo.ALIGN_CR,
									ghost = true,
								},

								{
									name = "savedate",
									class = "data",
									text = realdate
								},
							}, sender);
						end,

						select = function (sender, save)
							local selected = sender:parent():child("selected_savegame")
							-- deselect
							if (save == nil) then
								selected:set_text("")
								sender:parent():child("savegame_info"):set_invisible(true)
								return
							end

							-- selected savegame not found
							local savegame = sender:child(save)
							if (savegame == nil) then
								return
							end

							if ((selected:text() ~= nil) and selected:text() ~= "") then
								local previous_selection = sender:child(selected:text())
								previous_selection:child("title"):set_color(1, 1, 1, 0.5)
							end

							savegame:child("title"):set_color(1, 1, 1, 1)
							selected:set_text(savegame:name())

							local savegame_info = sender:parent():child("savegame_info")
							if (savegame_info ~= nil) then
								local filename = savegame:child("id"):text()
								savegame_info:child("gamedate"):set_text(savegame:child("gamedate"):text())
								savegame_info:child("savedate"):set_text(savegame:child("savedate"):text())
								if (filename == "") then
									savegame_info:child("delete"):set_disabled(true)
									ufo.cmd("set savegame_filename \"noname\";")
									ufo.cmd("set savegame_title \"no name\";")
								else
									savegame_info:child("delete"):set_disabled(false)
									ufo.cmd("set savegame_filename \"" .. filename:gsub(".savx", "") .. "\";")
									ufo.cmd("set savegame_title \"" .. savegame:child("title"):text() .. "\";")
								end
								savegame_info:set_invisible(false)
							end
						end,

						on_viewchange = function (sender)
							local scrollbar = sender:parent():child("savegame_list_scrollbar")
							scrollbar:set_fullsize(sender:fullsize())
							scrollbar:set_current(sender:viewpos())
							scrollbar:set_viewsize(sender:viewsize())
						end,

						on_wheel = function (sender)
							local scrollbar = sender:parent():child("savegame_list_scrollbar")
							scrollbar:set_current(sender:viewpos())
						end
					},

					{
						name = "savegame_list_scrollbar",
						class = "vscrollbar",
						image = "ui/scrollbar_v",
						pos = {465, 40},
						height = 320,
						current = 0,
						viewsize = 16,
						fullsize = 0,
						autoshowscroll = true,

						on_change = function (sender)
							local panel = sender:parent():child("savegame_list")
							panel:set_viewpos(sender:current())
						end
					},

					{
						name = "ui_clear_savegames",
						class = "confunc",
						on_click = function (sender, idx, title, gamedate, realdate, filename)
							sender:parent():child("savegame_list"):clear()
						end
					},
					{
						name = "ui_add_savegame",
						class = "confunc",
						on_click = function (sender, idx, title, gamedate, realdate, filename)
							if (filename == "") then
								title = "_--- new save ---"
							end
							sender:parent():child("savegame_list"):add(idx, title, gamedate, realdate, filename)
						end
					},

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
								local selected = tab:child("selected_savegame")
								local savegame = tab:child("savegame_list"):child(selected:text()):child("id")
								ufo.cmd(string.format("game_delete \"%s\";", savegame:text():gsub(".savx", "")))
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
								local selected = sender:parent():parent():child("selected_savegame")
								local savegame = sender:parent():parent():child("savegame_list"):child(selected:text()):child("id")
								ufo.cmd("web_uploadcgame 0 \"" .. savegame:text():gsub(".savx", "") .. "\"")
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
								local filename = ufo.findvar("savegame_filename"):as_string()
								local title = ufo.findvar("savegame_title"):as_string()
								ufo.cmd(string.format("game_save \"%s\" \"%s\";", filename, title))
								local tab = sender:parent():parent()
								tab:parent():child("tabset"):child(tab:name()):deselect()
								tab:parent():child("tabset"):child(tab:name()):select()
							end
						},
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
