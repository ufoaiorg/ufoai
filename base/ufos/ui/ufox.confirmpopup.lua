--!usr/bin/lua

--[[
-- @file
-- @brief Confirmation Popup builder extension
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

-- ufox.build_confirmpopup Header Guard
if (ufox.build_confirmpopup == nil) then

require("ufox.lua")

--[[
-- @brief Constructs a single-use confirmation popup
-- @param windowName name (identifier) of the window to create (must be unique)
-- @param properties Table with property key-value pairs to override defaults.
--	{
--		{
--			name = "title",
--			text = "<new title>",
--		},
--		{
--			name = "description",
--			text = "<new description",
--		},
--		{
--			name = "confirm",
--			on_click = function (sender)
--				ufo.print("Confirmed!")
--				ufo.pop_window(false)
--			end
--		},
--	}
--]]
ufox.build_confirmpopup = function (windowName, properties)
	local popupWindow = ufox.build_window({
		name = windowName,
		class = "window",
		pos = {256, 256},
		size = {512, 190},
		backgroundcolor = {0, 0, 0, 0.6},
		bordercolor = {0.56, 0.81, 0.76, 1},
		bordersize = 2,
		dragbutton = true,
		modal = true,
		closebutton = false,
		preventtypingescape = true,

		on_windowclosed = function (sender)
			ufo.delete_node(sender)
		end,

		{
			name = "title",
			class = "string",
			text = "Confirmation needed",
			pos = {0, 0},
			size = {512, 30},
			font = "f_small_bold",
			contentalign = ufo.ALIGN_CL,
			color = {0.56, 0.81, 0.76, 1},
			backgroundcolor = {0.4, 0.515, 0.5, 0.25}
		},

		{
			name = "description",
			class = "text",
			text = "Are you sure?",
			pos = {5, 55},
			size = {480, 100},

			on_viewchange = function (sender)
				local scrollbar = sender:parent():child("description_scroll")
				scrollbar:set_fullsize(sender:fullsize())
				scrollbar:set_current(sender:viewpos())
				scrollbar:set_viewsize(sender:viewsize())
			end,

			on_wheel = function (sender)
				local scrollbar = sender:parent():child("description_scroll")
				scrollbar:set_current(sender:viewpos())
			end
		},

		{
			name = "description_scroll",
			class = "vscrollbar",
			image = "ui/scrollbar_v",
			pos = {490, 55},
			height = 100,
			current = 0,
			viewsize = 5,
			fullsize = 0,
			autoshowscroll = true,

			on_change = function (sender)
				local textNode = sender:parent():child("description")
				panel:set_viewpos(sender:current())
			end
		},

		{
			name = "confirm",
			class = "MainMenuBtn",
			text = "_Confirm",
			pos = {142, 160},
			size = {180, 30},

			on_click = function (sender)
				ufo.pop_window(false)
			end,
		},

		{
			name = "cancel",
			class = "MainMenuBtn",
			text = "_Cancel",
			pos = {332, 160},
			size = {180, 30},

			on_click = function (sender)
				ufo.pop_window(false)
			end,
		},
	});
	ufox.build_properties(properties, nil, popupWindow)
	return popupWindow
end

-- ufox.build_confirmpopup
end
