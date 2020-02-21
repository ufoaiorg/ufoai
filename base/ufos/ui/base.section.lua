--!usr/bin/lua

--[[
-- @file
-- @brief Base menu section builder
--]]

--[[
Copyright (C) 2002-2020 UFO: Alien Invasion.

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

-- header guard
if (base.build_section == nil) then

require("ufox.lua")

base.build_section = function (root_node, id, title, icon)
	local section = root_node:child(id)
	if (section ~= nil) then
		-- delete (and later re-build) section
		section:delete_node()
	end

	section = ufox.build({
		name = id,
		class = "panel",
		size = {root_node:width(), 48},

		{
			name = "header",
			class = "panel",
			size = {root_node:width(), 48},

			{
				name = "background_stripe",
				class = "panel",
				pos = {0, 8},
				size = {root_node:width(), 32},
				backgroundcolor = {0.527, 0.6, 0.21, 0.2},
				ghost = true,

				{
					name = "title",
					class = "string",
					text = title,
					pos = {75, 0},
					size = {root_node:width() - 75, 32},
					contentalign = ufo.ALIGN_CL,
					color = {0.56, 0.81, 0.76, 1},
					ghost = true,
				},
			},

			on_mouseenter = function (sender)
				if (sender:is_disabled()) then
					return
				end
				if (sender:child("background_stripe") ~= nil) then
					sender:child("background_stripe"):set_backgroundcolor(0.527, 0.6, 0.21, 0.4)
				end
				if (sender:child("icon") ~= nil) then
					sender:child("icon"):set_background("icons/circle")
				end
			end,

			on_mouseleave = function (sender)
				if (sender:is_disabled()) then
					return
				end
				if (sender:child("background_stripe") ~= nil) then
					sender:child("background_stripe"):set_backgroundcolor(0.527, 0.6, 0.21, 0.2)
				end
				if (sender:child("icon") ~= nil) then
					sender:child("icon"):set_background("icons/circle0")
				end
			end,
		},

		{
			name = "content",
			class = "panel",
			pos = {0, 48},
			size = {root_node:width(), 0},
			layout = ufo.LAYOUT_TOP_DOWN_FLOW,
			layoutmargin = 0,

			update_height = function (sender)
				local height = 0
				local child = sender:first()
				while (child ~= nil) do
					if (sender:layout() == ufo.LAYOUT_TOP_DOWN_FLOW) then
						height = height + child:height()
					else
						if (child:height() > height) then
							height = child:height()
						end
					end
					child = child:next()
				end
				sender:set_height(height)
				if (sender:parent().update_height ~= nil) then
					sender:parent():update_height()
				end
			end,

			on_viewchange = function (sender)
				sender:update_height()
			end,
		},


		disable = function (sender, tooltip)
			sender:child("header"):set_tooltip(tooltip)
			sender:child("header"):child("background_stripe"):set_backgroundcolor(1, 1, 1, 0.2)
			sender:child("header"):set_disabled(true)
		end,
		enable = function (sender, tooltip)
			sender:child("header"):set_tooltip(tooltip)
			sender:child("header"):child("background_stripe"):set_backgroundcolor(0.527, 0.6, 0.21, 0.2)
			sender:child("header"):set_disabled(false)
		end,

		update_height = function (sender)
			sender:set_height(sender:child("header"):height() + sender:padding() + sender:child("content"):height())
		end,
	}, root_node)

	-- build separately to make sure it is above the background stripe
	local icon = ufox.build({
		name = "icon",
		class = "button",
		pos = {12, 0},
		size = {48, 48},
		icon = icon,
		background = "icons/circle0",
		ghost = true,

		-- else the button is disabled
		on_click = function (sender)
		end,
	}, section:child("header"))

	return section
end

-- header guard
end
