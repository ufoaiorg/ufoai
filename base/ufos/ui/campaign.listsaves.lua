--!usr/bin/lua

--[[
-- @file
-- @brief UI component that lists (local) savegames
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

-- campaign.listsaves Header Guard
if (campaign.listsaves == nil) then

require("ufox.lua")
require("campaign.saveinfo.lua")

campaign.listsaves = function (rootNode)
	local ui = ufox.build({
		name = "locallist",
		class = "panel",
		pos = {0, 0},
		size = {495, 480},

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

			search = function (sender, filename)
				if (filename == nil or filename == "") then
					return
				end

				local save = sender:first()
				while (save ~= nil) do
					if (save:child("id"):text() == filename) then
						return save
					end
					save = save:next()
				end

				return nil
			end,

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
						text = filename
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
				campaign.saveinfo.close(rootNode)
				if (save == nil) then
					selected:set_text("")
					return
				end
				if ((selected:text() ~= nil) and selected:text() ~= "") then
					local previous_selection = sender:child(selected:text())
					previous_selection:child("title"):set_color(1, 1, 1, 0.5)
				end

				-- select
				local savegame = sender:child(save)
				savegame:child("title"):set_color(1, 1, 1, 1)
				selected:set_text(savegame:name())
				campaign.saveinfo.open(rootNode, savegame, rootNode:name())
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
	}, rootNode)
	return ui
end

-- campaign.listsaves Header Guard
end
