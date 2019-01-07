--!usr/bin/lua

--[[
-- @file
-- @brief Initial settings popup
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

if (initialsettings == nil) then

require("ufox.lua")

initialsettings = {}

initialsettings.check = function ()
	local cl_name = ufo.getvar("cl_name")
	if (cl_name == nil or cl_name:as_string() == "") then
		return false
	end

	local s_language = ufo.getvar("s_language")
	if (s_language == nil or s_language:as_string() == "") then
		return false
	end

	return true
end

initialsettings.build = function ()
	local settings = ufox.build_window({
		name = "initial_settings",
		class = "window",
		super = "ipopup",
		modal = true,
		preventtypingescape = true,
		pos = {124, 192},
		size = {777, 368},
		backgroundcolor = {0, 0, 0, 0.9},

		validate_form = function (sender)
			local valid = true
			local ok_button = sender:child("ok")

			local username = ufo.getvar("cl_name")
			local username_field = sender:child("username")
			if (username == nil or string.match(username:as_string(), "^[ \t\r\n]*$")) then
				username_field:set_bordersize(2)
				username_field:set_tooltip("_Player name cannot be empty")
				valid = false
			else
				username_field:set_bordersize(0)
				username_field:set_tooltip("")
			end

			local language = ufo.getvar("s_language")
			local language_field = sender:child("language_select")
			if (language == nil or string.match(language:as_string(), "^\s*$")) then
				language_field:set_bordersize(2)
				language_field:set_tooltip("_Please select your preferred language")
				valid = false
			else
				language_field:set_bordersize(0)
				language_field:set_tooltip("")
			end

			ok_button:set_disabled(not valid)
			return valid
		end,

		{
			name = "title",
			text = "_Initial settings",
			size = {777, 30},
		},

		-- USER INFORMATION
		{
			name = "userinfo_label",
			class = "string",
			text = "_User information",
			pos = {26, 48},
			size = {300, 30},
			font = "f_normal",
		},

		{
			name = "username_label",
			class = "string",
			text = "_Name:",
			pos = {41, 88},
			size = {300, 20},
			color = {1, 1, 1, 1},
		},

		{
			name = "username",
			class = "textentry",
			text = "*cvar:cl_name",
			font = "f_verysmall",
			pos = {170, 82},
			size = {185, 34},
			backgroundcolor = {0, 0, 0, 0.5},
			bordercolor = {1, 0, 0, 1},
			bordersize = 0,

			on_keyreleased = function (sender)
				sender:root():validate_form()
			end,
		},

		-- VOLUME SETTINGS
		{
			name = "volume_str",
			class = "string",
			text = "_Volume Control",
			pos = {26, 218},
			size = {390, 25},
			font = "f_normal",
		},

		{
			name = "effects_str",
			class = "string",
			text = "_Effects:",
			pos = {41, 248},
			size = {150, 20},
		},

		{
			name = "effects_bar",
			class = "bar",
			value = "*cvar:snd_volume",
			pos = {170, 258},
			size = {238, 6},
			color = {0.582, 0.808, 0.758, 1},
			bordercolor = {0.582, 0.808, 0.758, 1},
			bordersize = 1,
			max = 1.0,
		},

		{
			name = "music_str",
			class = "string",
			text = "_Music:",
			pos = {41, 273},
			size = {150, 20},
		},

		{
			name = "music_bar",
			class = "bar",
			value = "*cvar:snd_music_volume",
			pos = {170, 283},
			size = {238, 6},
			color = {0.582, 0.808, 0.758, 1},
			bordercolor = {0.582, 0.808, 0.758, 1},
			bordersize = 1,
			max = 128.0,
		},

		-- LANGUAGE
		{
			name = "language_settings",
			class = "string",
			text = "_Language",
			pos = {460, 48},
			size = {300, 25},
			font = "f_normal",
		},

		{
			-- @TODO eliminate optionlists
			name = "language_select",
			class = "optionlist",
			pos = {461, 78},
			size = {275, 226},
			font = "f_language",
			--background = "ui/panel",
			color = {1, 1, 1, 0.5},
			selectcolor = {1, 1, 1, 0.9},
			backgroundcolor = {0, 0, 0, 0.5},
			bordercolor = {1, 0, 0, 1},
			bordersize = 0,
			padding = 6,
			contentalign = ufo.ALIGN_CC,
			dataid = ufo.OPTION_LANGUAGES,
			cvar = "*cvar:s_language",

			on_change = function (sender)
				sender:root():validate_form()
			end,

			on_viewchange = function (sender)
				sender:root():child("language_select_scroll"):set_current(sender:current())
				sender:root():child("language_select_scroll"):set_fullsize(sender:fullsize())
				sender:root():child("language_select_scroll"):set_viewsize(sender:viewsize())
			end,
		},

		{
			name = "language_select_scroll",
			class = "vscrollbar",
			pos = {742, 78},
			height = 226,
			viewsize = 8,
			fullsize = 21,
			current = 0,
			image = "ui/scrollbar_v",

			on_change = function (sender)
				sender:root():child("language_select"):set_current(sender:current())
			end,
		},

		{
			name = "message",
			class = "string",
			text = "_You have to set the playername and hit the 'OK' button:",
			pos = {41, 338},
			size = {500, 20},
			color = {1, 1, 1, 0.5},
		},

		{
			name = "ok",
			class = "MainMenuBtn",
			text = "_OK",
			tooltip = "_Save settings",
			pos = {547, 338},
			size = {230, 30},
			on_click = function (sender)
				ufo.cmd("saveconfig config.cfg;")
				ufo.pop_window(false)
			end,
		},

		{
			class = "fuzzyScreen",
			name = "overlay",
		},

		on_windowopened = function (sender)
			sender:validate_form()
		end,

		on_windowclosed = function (sender)
			sender:delete_node()
		end,
	})
	return settings
end

end
