--!usr/bin/lua

--[[
-- @file
-- @brief UI component that shows detailed info about a savegame
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

-- campaign.saveinfo Header Guard
if (campaign.saveinfo == nil) then

require("ufox.lua")
require("ufox.confirmpopup.lua")

campaign.saveinfo = {
	--[[
	-- @brief Checks if savegame filaname is valid and update controls
	-- @param[in, out] sender Savegame filename node
	--]]
	check_savegame_filename = function (sender)
		local filename = ufo.getvar("savegame_filename")
		if (filename == nil) then
			return
		end

		local savelist = sender:parent():parent():find("savegame_list")
		local save_exists = (savelist ~= nil and savelist:search(filename:as_string()) ~= nil)

		local delete_button = sender:parent():child("delete")
		if (delete_button ~= nil) then
			if (not save_exists) then
				delete_button:set_tooltip("_File doesn't exists")
				delete_button:set_disabled(true)
			else
				delete_button:set_tooltip("")
				delete_button:set_disabled(false)
			end
		end

		local save_button = sender:parent():child("save")
		if (save_button ~= nil) then
			if (string.match(filename:as_string(), "^[a-zA-Z0-9_-]+$") == nil) then
				sender:set_bordersize(2)
				sender:set_tooltip("_File name must be constructed from alphanumeric characters, hypens and underscores")
				save_button:set_disabled(true)
			else
				sender:set_bordersize(0)
				sender:set_tooltip("")
				save_button:set_disabled(false)
			end
		end
	end,

	--[[
	-- @brief Checks if savegame title is valid and update controls
	-- @param[in, out] sender Savegame title node
	--]]
	check_savegame_title = function (sender)
		local title = ufo.getvar("savegame_title")
		if (title == nil) then
			return
		end

		local save_button = sender:parent():child("save")
		if (save_button == nil) then
			return
		end

		if (title:as_string():len() == 0) then
			sender:set_bordersize(2)
			sender:set_tooltip("_Title cannot not be empty")
			save_button:set_disabled(true)
			return
		end

		sender:set_tooltip("")
		sender:set_bordersize(0)
		save_button:set_disabled(false)
	end,

	open = function (rootNode, savegame, mode)
		local title = savegame:child("title"):text()
		ufo.getvar("savegame_title", title):set_value(title)

		local filename = savegame:child("id"):text()
		local exists = true
		if (filename == nil or filename == "") then
			exists = false
			filename = os.date("%Y%m%d-%H%M%S")
		end
		ufo.getvar("savegame_filename", filename):set_value(filename)

		local node_class
		local node_background
		if (mode == "save") then
			node_class = "textentry"
			node_background = {0, 0, 0, 0.5}
		else
			node_class = "string"
			node_background = {0, 0, 0, 0}
		end

		local saveinfo = ufox.build({
			name = "savegame_info",
			class = "panel",
			pos = {490, 40},
			size = {534, 360},

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
				class = node_class,
				text = "*cvar:savegame_filename",
				pos = {205, 5},
				size = {315, 20},
				color = {1, 1, 1, 0.5},
				backgroundcolor = node_background,
				bordercolor = {1, 0, 0, 1},
				bordersize = 0,

				on_keyreleased = campaign.saveinfo.check_savegame_filename,
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
				class = node_class,
				text = "*cvar:savegame_title",
				pos = {205, 35},
				size = {315, 20},
				color = {1, 1, 1, 0.5},
				backgroundcolor = node_background,
				bordercolor = {1, 0, 0, 1},
				bordersize = 0,

				on_keyreleased = campaign.saveinfo.check_savegame_title,
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
				text = savegame:child("gamedate"):text(),
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
				text = savegame:child("savedate"):text(),
				pos = {205, 95},
				size = {315, 20},
				color = {1, 1, 1, 0.5},
			},

			{
				name = "upload",
				class = "MainMenu3Btn",
				text = "_UPLOAD",
				pos = {284, 250},
				size = {250, 30},
				backgroundcolor = {0.38, 0.48, 0.36, 1},
				--tooltip = "_Upload the savegame to the UFO:AI Server",
				tooltip = "WebUpload functionality is disabled temporarily",
				disabled = true,

				on_mouseenter = function (sender)
					if (sender:is_disabled()) then
						return
					end
					sender:set_backgroundcolor(0.59, 0.78, 0.56, 1)
				end,
				on_mouseleave = function (sender)
					if (sender:is_disabled()) then
						return
					end
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
				name = "delete",
				class = "MainMenu3Btn",
				text = "_DELETE",
				pos = {284, 290},
				size = {250, 30},
				backgroundcolor = {0.38, 0.48, 0.36, 1},
				disabled = (not exists),

				on_mouseenter = function (sender)
					if (sender:is_disabled()) then
						return
					end
					sender:set_backgroundcolor(0.59, 0.78, 0.56, 1)
				end,
				on_mouseleave = function (sender)
					if (sender:is_disabled()) then
						return
					end
					sender:set_backgroundcolor(0.38, 0.48, 0.36, 1)
				end,

				on_click = function (sender)
					local tab = sender:parent():parent()
					local filename = ufo.getvar("savegame_filename")
					if (filename == nil or filename == "") then
						return
					end
					if (filename == nil) then
						return
					end

					local popup = ufox.build_confirmpopup("popup_deletesave", {
						{
							name = "title",
							text = "_Delete saved game",
						},
						{
							name = "description",
							text = "_Do you really want to detete this saved game?",
						},
						{
							name = "confirm",
							text = "_Delete",

							on_click = function (sender)
								ufo.cmd(string.format("game_delete \"%s\";", filename:as_string()))
								tab:parent():child("tabset"):child(tab:name()):deselect()
								tab:parent():child("tabset"):child(tab:name()):select()
								ufo.pop_window(false)
							end,
						},
					})
					ufo.push_window(popup:name(), nil, nil)
				end,
			}
		}, rootNode)

		if (mode == "save") then
			local save_button = ufox.build({
				name = "save",
				class = "MainMenu3Btn",
				text = "_SAVE",
				pos = {284, 330},
				size = {250, 30},
				backgroundcolor = {0.38, 0.48, 0.36, 1},

				on_mouseenter = function (sender)
					if (sender:is_disabled()) then
						return
					end
					sender:set_backgroundcolor(0.59, 0.78, 0.56, 1)
				end,
				on_mouseleave = function (sender)
					if (sender:is_disabled()) then
						return
					end
					sender:set_backgroundcolor(0.38, 0.48, 0.36, 1)
				end,

				on_click = function (sender)
					local tab = sender:parent():parent()
					local filename = ufo.getvar("savegame_filename")
					if (filename == nil or filename == "") then
						return
					end
					local title = ufo.getvar("savegame_title")
					if (title == nil or title == "") then
						return
					end

					local delete_button = sender:parent():child("delete")
					if (delete_button:is_disabled() == false) then
						local popup = ufox.build_confirmpopup("popup_overwritesave", {
							{
								name = "title",
								text = "_Overwrite saved game",
							},
							{
								name = "description",
								text = "_Do you really want to overwrite this saved game?",
							},
							{
								name = "confirm",
								text = "_Overwrite",

								on_click = function (sender)
									ufo.cmd(string.format("game_save \"%s\" \"%s\";", filename:as_string(), title:as_string()))
									tab:parent():child("tabset"):child(tab:name()):deselect()
									tab:parent():child("tabset"):child(tab:name()):select()
									ufo.pop_window(false)
								end,
							},
						})
						ufo.push_window(popup:name(), nil, nil)
					else
						ufo.cmd(string.format("game_save \"%s\" \"%s\";", filename:as_string(), title:as_string()))
						tab:parent():child("tabset"):child(tab:name()):deselect()
						tab:parent():child("tabset"):child(tab:name()):select()
					end
				end
			}, saveinfo)
			campaign.saveinfo.check_savegame_filename(saveinfo:child("filename"))
			campaign.saveinfo.check_savegame_title(saveinfo:child("title"))
		else
			local begin_button = ufox.build({
				name = "begin",
				class = "MainMenu3Btn",
				text = "_BEGIN",
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
					ufo.cmd(string.format("game_load \"%s\";", filename:as_string()))
				end
			}, saveinfo)
		end
	end,

	close = function (rootNode)
		local saveinfo = rootNode:child("savegame_info")
		if (saveinfo == nil) then
			return
		end
		saveinfo:remove_children()
		ufo.delete_node(saveinfo)
		ufo.delvar("savegame_filename")
		ufo.delvar("savegame_title")
	end
}
-- campaign.saveinfo Header Guard
end
