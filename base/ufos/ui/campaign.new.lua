--!usr/bin/lua

--[[
-- @file
-- @brief Start new campaign main menu tab definition
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

-- campaign.new Header Guard
if (campaign.new == nil) then

require("ufox.lua")

campaign.new = {
	register = function (rootNode, tabset)
		local button = ufox.build({
			name = "new",
			class = "MainMenuTab",
			text = "_NEW",
			width = 135,

			create = function (sender, nodeName, rootNode)
				local tab = ufox.build({
					name = nodeName,
					class = "panel",
					pos = {0, 35},
					size = {1024, 400},

					-- Left side
					{
						name = "selected_campaign",
						class = "data",
						text = "",
					},

					{
						name = "campaign_list",
						class = "panel",
						pos = {10, 40},
						size = {350, 320},
						layout = ufo.LAYOUT_TOP_DOWN_FLOW,
						wheelscrollable = true,
						backgroundcolor = {0, 0, 0, 0.5},

						clear = function (sender)
							sender:remove_children()
						end,

						add = function(sender, id, name, default)
							local new_node = ufox.build({
								name = id,
								class = "string",
								size = {340, 20},
								text = name,
								color = {1, 1, 1, 0.5},

								select = function (sender)
									local selected = sender:parent():parent():child("selected_campaign")
									if (selected:text() ~=  "" and selected:text() ~= sender:name()) then
										local selected_campaign = sender:parent():child(selected:text())
										if (selected_campaign ~= nil) then
											selected_campaign:set_color(1, 1, 1, 0.5)
										end
									end
									sender:set_color(1, 1, 1, 1)
									selected:set_text(sender:name())
									ufo.cmd("cp_getdescription \"" .. sender:name() .. "\";")
								end,

								on_click = function (sender)
									sender:select()
								end
							}, sender)
							if (default == "default") then
								sender:child(id):select()
							end
						end,

						on_viewchange = function (sender)
							local scrollbar = sender:parent():child("campaign_list_scrollbar")
							scrollbar:set_fullsize(sender:fullsize())
							scrollbar:set_current(sender:viewpos())
							scrollbar:set_viewsize(sender:viewsize())
						end,

						on_wheel = function (sender)
							local scrollbar = sender:parent():child("campaign_list_scrollbar")
							scrollbar:set_current(sender:viewpos())
						end,
					},

					{
						name = "campaign_list_scrollbar",
						class = "vscrollbar",
						image = "ui/scrollbar_v",
						pos = {365, 40},
						height = 320,
						current = 0,
						viewsize = 16,
						fullsize = 0,
						autoshowscroll = true,

						on_change = function (sender)
							local panel = sender:parent():child("campaign_list")
							panel:set_viewpos(sender:current())
						end
					},

					{
						name = "ui_clear_campaigns",
						class = "confunc",
						on_click = function (sender)
							local list = sender:parent():child("campaign_list")
							if (list ~= nil) then
								list:clear()
							end
						end
					},

					{
						name = "ui_add_campaign",
						class = "confunc",
						on_click = function (sender, id, name, default)
							local list = sender:parent():child("campaign_list")
							if (list ~= nil) then
								list:add(id, name, default)
							end
						end
					},

					-- Right side
					{
						name = "campaign_description",
						class = "text",
						pos = {400, 40},
						size = {589, 320},
						dataid = ufo.TEXT_STANDARD,
						color = {1, 1, 1, 0.5},
						disabledcolor = {1, 1, 1, 0.5},
						lineheight = 20,
						disabled = true,

						on_viewchange = function (sender)
							local scrollbar = sender:parent():child("campaign_description_scrollbar")
							scrollbar:set_fullsize(sender:fullsize())
							scrollbar:set_current(sender:viewpos())
							scrollbar:set_viewsize(sender:viewsize())
						end,

						on_wheel = function (sender)
							local scrollbar = sender:parent():child("campaign_description_scrollbar")
							scrollbar:set_current(sender:viewpos())
						end
					},

					{
						name = "campaign_description_scrollbar",
						class = "vscrollbar",
						image = "ui/scrollbar_v",
						pos = {994, 40},
						height = 320,
						current = 0,
						viewsize = 16,
						fullsize = 0,
						autoshowscroll = true,

						on_change = function (sender)
							local panel = sender:parent():child("campaign_description")
							panel:set_viewpos(sender:current())
						end
					},

					{
						name = "begin",
						class = "MainMenu3Btn",
						text = "_BEGIN",
						pos = {774, 370},
						size = {250, 30},
						backgroundcolor = {0.38, 0.48, 0.36, 1},

						on_mouseenter = function (sender)
							sender:set_backgroundcolor(0.59, 0.78, 0.56, 1)
						end,

						on_mouseleave = function (sender)
							sender:set_backgroundcolor(0.38, 0.48, 0.36, 1)
						end,

						on_click = function (sender)
							local selected = sender:parent():child("selected_campaign")
							ufo.cmd("cp_start \"" .. selected:text() .. "\"")
						end
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
				-- (re-)load campaigns
				ufo.cmd("cp_getcampaigns;")
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

-- campaign.new Header Guard
end
