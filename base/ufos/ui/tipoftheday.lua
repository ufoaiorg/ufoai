--!usr/bin/lua

--[[
-- @file
-- @brief Tip of the day popup
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

if (tipoftheday == nil) then

require("ufox.lua")

tipoftheday = {}

tipoftheday.build = function ()
	local popup = ufox.build_window({
		class = "window",
		name = "tipoftheday",
		super = "ipopup",

		pos = {518, 578},
		size = {506, 190},
		closebutton = true,
		dragbutton = true,
		backgroundcolor = {0, 0, 0, 0.9},

		{
			name = "title",
			width = 496,
			text = "_Tip of the day",
		},

		{
			class = "string",
			name = "activate_str",
			text = "_Show tip of the day",
			pos = {58, 166},
			size = {200, 25},
			font = "f_small",

			on_click = function (sender)
				sender:root():child("activate"):toggle()
			end,
		},

		{
			class = "CheckBox",
			name = "activate",
			value = "*cvar:cl_showTipOfTheDay",
			pos = {36, 170},
			size = {20, 18},

			on_change = function (sender)
				ufo.cmd("do_nexttip;")
			end,
		},

		{
			class = "text",
			name = "tipoftheday_text",
			pos = {26, 48},
			size = {444, 100},
			lineheight = 20,
			tabwidth = 150,
			dataid = ufo.TEXT_TIPOFTHEDAY,
			viewsize = 5,
		},

		{
			class = "MainMenuBtn",
			name = "nexttip",
			text = "_Next",
			tooltip = "_Next tip",
			width = 176,
			pos = {319, 160},
			font = "f_menu",

			on_click = function (sender)
				if (ufo.getvar("cl_showTipOfTheDay"):as_integer() == 1) then
					ufo.cmd("tipoftheday;")
				else
					ufo.cmd("ui_close tipoftheday;")
				end
			end,
		},

		{
			class = "confunc",
			name = "do_nexttip",

			on_click = function (sender)
				if (ufo.getvar("cl_showTipOfTheDay"):as_integer() == 1) then
					sender:root():child("nexttip"):set_text("_Next")
					sender:root():child("nexttip"):set_tooltip("_Next tip")
				else
					sender:root():child("nexttip"):set_text("_Close")
					sender:root():child("nexttip"):set_tooltip("_Close window")
				end
			end,
		},

		{
			class = "fuzzyScreen",
			name = "overlay",
		},
	})

	return popup
end

end
