--!usr/bin/lua

--[[
-- @file
-- @brief LUA extensions to the UI API
-- @note Only generic functions should be here any specific implementation (like new nodes) should be separated
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

-- ufox Header Guard
if (ufox == nil) then

ufox = {}	--< namespace of lua extensions


--[[
-- @brief Sets properties for an existing UI Node
-- @param data Table of property key-value pairs to set
-- @param parent Parent UI Node reference
-- @param ui_node UI Node to set the properties for
--]]
function ufox.build_properties (data, parent, ui_node)
	if (ui_node == nil) then
		return nil
	end

	-- iterate all the other properties in the table
	for k, v in pairs(data) do
		if (k ~= "class") and (k ~= "name") and (k ~= "super") then

			-- if it is again a table with a class field specified, then the table is used to create a new child node of the current node
			if (type(v) == "table") and (v.class ~= nil) and (v.name ~= nil) then
				-- recurse call
				ufox.build(v, ui_node)

			-- if it is a table without a class field, then the name is used to lookup an existing child node of the current node
			elseif (type(v) == "table") and (v.class == nil) and (v.name ~= nil) then
				-- get the child node using the name
				local ui_child = ui_node:child(v.name)
				-- if a node is found, then fill properties
				if (ui_child ~= nil) then
					ufox.build_properties(v, ui_node, ui_child)
				end

			-- for a function, use the function directly
			elseif (type(v) == "function") then
				-- set the event handler directly
				ui_node[k] = v

			-- in every other case, the value specified should be set directly
			else
				-- translate key to a property set function
				local fn_set = string.format("set_%s", tostring(k))
				if (ui_node[fn_set] == nil) then
					ufo.print(string.format('[ufox.build] WARNING: Unrecognized property: "%s" ignored (%s)\n', tostring(k), ufo.nodepath(ui_node)))
				else
					if (type(v) == "table") then
						-- multiple value argument: should be a table of values
						ui_node[fn_set](ui_node, unpack(v))
					else
						-- single value argument: should be a single value
						ui_node[fn_set](ui_node, v)
					end
				end
			end
		end
	end
end


--[[
-- @brief Creates a UI Node (tree) from a data table hierarchy
-- @param data Hierarchy of tables describing UI Node tree
-- @param parent Parent UI Node reference
-- @return reference to the just created UI Node table
-- @note If a node already exists on the parent, it is reused
--]]
function ufox.build (data, parent)
	if (type(data) ~= "table") then
		return nil
	end

	if (data.class ~= nil) and (data.name ~= nil) then
		local ui_node = ufo.create_control(parent, data.class, data.name, data.super)
		if (not ui_node) then
			ufo.print(string.format('[ufox.build] ERROR: Could not create ui node "%s"', data.name))
			return nil
		end
		ufox.build_properties(data, parent, ui_node)

		return ui_node
	else
		ufo.print(string.format("[ufox.build] ERROR: Mandatory field class (%s) or name (%s) not found for creating node under (%s)\n", tostring(data.class), tostring(name), ufo.nodepath(parent)))
	end
	return nil
end


--[[
-- @brief Creates a UI Window with all its contents from a data table hierarchy
-- @param data Hierarchy of tables describing UI Node tree from the window
-- @return reference to the just created UI Window Node table
-- @TODO Merge this into ufox.build()
--]]
function ufox.build_window (data)
	if (type(data) ~= "table") then
		return nil
	end

	if (data.class ~= nil) and (data.name ~= nil) then
		if (data.class ~= "window") then
			ufo.print("[ufox.build_window] WARNING: a window node is being requested but the top level class is not set to 'window'\n")
		end

		local ui_window = ufo.create_window(data.name, data.super)
		if (not ui_window) then
			ufo.print(string.format('[ufox.build_window] ERROR: Could not create window "%s"', data.name))
			return nil
		end
		ufox.build_properties(data, parent, ui_window)

		return ui_window
	else
		ufo.print("[ufox.build_window] ERROR: fields class and name expected but not found\n")
	end

	return nil
end


--[[
-- @brief Creates a UI Component Node (tree) from a data table hierarchy
-- @param data Hierarchy of tables describing UI Node tree
-- @return reference to the just created UI Node table
-- @TODO The Component nodes are DEPRECATED. LUA functions should act as Node tree templates.
--]]
function ufox.build_component (data)
	if (type(data) ~= "table") then
		return nil
	end

	if (data.class ~= nil) and (data.name ~= nil) then
		if (data.class == "window") then
			ufo.print("[ufox.build_component] WARNING: a component node is being requested but the top level class is set to 'window'\n")
		end

		local ui_component = ufo.create_component(data.class, data.name, data.super)
		if (not ui_component) then
			ufo.print(string.format('[ufox.build_component] ERROR: Could not create component "%s"', data.name))
			return nil
		end
		ufox.build_properties(data, parent, ui_component)

		return ui_component
	else
		ufo.print("[ufox.build_component] ERROR: fields class and name expected but not found\n")
	end

	return nil
end

-- ufox Header Guard
end
