--!usr/bin/lua

--[[
-- @file
-- @brief TabSet builder extension for the Main Menu screens
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

-- ufox.build_maintabset Header Guard
if (ufox.build_maintabset == nil) then

require("ufox.lua")

--[[
-- @brief Constructs a tabset for Main Menu
-- @param rootnode Root Panel node the tabset should be created in
-- @param properties Table with optional property key-value pairs to override defaults. Use empty table @c {} for no override.
-- @params ... Tables describing the Tabs:
--	Each table should contain a register(rootnode, tabset) function which builds the button that selects the tab.
--	The button should have
--	- select(sender) function which
--		- higlights the button
--		- creates/unhides the tab content
--	- deselect(sender) function
--		- removes the highlight from the button
--		- hides/destroys the tab content
--	The on_click() event of the button will be overridden by the tabset. Don't define any actions there!
--]]
function ufox.build_maintabset (rootnode, properties, ...)
	local tabs = ufox.build({
		name = "tabset",
		class = "panel",
		pos = {0, 0},
		size = {rootnode:width(), 30},
		layout = ufo.LAYOUT_LEFT_RIGHT_FLOW,
		layoutmargin = 5,

		{
			name = "selection",
			class = "data",
			text = "",
		},

		select = function (sender, tab)
			local selection = sender:child("selection")
			if (selection:text() ~= nil) and (selection:text() ~= "") then
				local selected_button = sender:child(selection:text())

				if (selected_button ~= nil) then
					selected_button:deselect()
				end
				selection:set_text("");
			end

			local new_button = sender:child(tab)
			if (new_button ~= nil) then
				new_button:select()
				selection:set_text(new_button:name())
			end
		end,

	}, rootnode)

	-- Allow override properties
	if (properties ~= nil) and (type(properties) == "table") then
		ufox.build_properties(properties, rootnode, tabs)
	end

	local selection = tabs:child("selection")
	for i, v in ipairs(arg) do
		local button = v.register(rootnode, tabs)
		button.on_click = function (sender)
			tabs:select(sender:name())
		end
	end
end

-- ufox.build_maintabset Header Guard
end
