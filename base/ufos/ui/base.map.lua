--!usr/bin/lua

--[[
-- @file
-- @brief Build base map
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

-- header guard
if (base.buildmap == nil) then

require("ufox.lua")

--[[
 - @brief Build a base map
 - @param root_node Panel node to build the map into
 - @param base_idx Numeric index of the base to show
--]]
base.buildmap = function (sender, root_node, base_idx)
	local map = root_node:child("map")
	if (map ~= nil) then
		map:delete_node()
	end

	map = ufox.build({
		name = "map",
		class = "panel",
		pos = {0, 0},
		size = {root_node:width(), root_node:height()},

		{
			name = "build",
			class = "string",
			size = {0, 0},
			text = "",
			invisible = true,
		},

		--[[
		 - @brief Add a building / tile to the base
		 - @param x Horizontal position of the tile by tiles
		 - @param y Vertical position of the tile by tiles
		 - @param width Horizontal size of the tile by tiles
		 - @param height Vertical size of the tile by tiles
		 - @param image Image to show
		 - @param buildTime Translated build time string
		--]]
		add = function (sender, x, y, width, height, id, name, image, buildTime)
			local tile_width = math.floor(sender:width() / 5)
			local tile_height = math.floor(sender:height() / 5)

			local tile = sender:child(string.format("tile_%d_%d", x, y))
			if (tile ~= nil) then
				tile:delete_node()
			end
			local tile = ufox.build({
				name = string.format("tile_%d_%d", x, y),
				class = "image",
				pos = {x * tile_width, y * tile_height},
				size = {width * tile_width, height * tile_height},
				source = image,

				{
					name = "building_id",
					class = "text",
					text = id,
					pos = {x, y},
					size = {width, height},
					invisible = true,
				},

				{
					name = "construction",
					class = "string";
					text = buildTime,
					font = "f_small",
					pos = {0, height * tile_height - 30},
					size = {width * tile_width, 20},
					contentalign = ufo.ALIGN_CR,
					ghost = true,
				},

				{
					name = "action",
					class = "panel",
					pos = {0, 0},
					size = {width * tile_width, height * tile_height},
					backgroundcolor = {0, 0, 0, 0},
					tooltip = name,

					--[[
					 - @brief Highlight an area for a building
					 - @param width Building width in tiles
					 - @param height Building height in tiles
					 - @param on If the tiles should be highlighted or not (boolean)
					--]]
					highlight = function (sender, width, height, on)
						local posX = sender:parent():child("building_id"):left()
						local posY = sender:parent():child("building_id"):top()
						for j = posY, posY + height - 1 do
							for i = posX, posX + width - 1 do
								local tile = sender:parent():parent():child(string.format("tile_%d_%d", i, j))
								if (tile ~= nil) then
									if (on) then
										tile:child("action"):set_backgroundcolor(0.4, 0.52, 0.5, 0.4)
									else
										tile:child("action"):set_backgroundcolor(0, 0, 0, 0)
									end
								end
							end
						end
					end,

					on_click = function (sender)
						local base_idx = sender:root():child("base_idx")
						local build = sender:parent():parent():child("build")
						local building = sender:parent():child("building_id")
						-- "normal" info mode
						if (build:text() == "") then
							if (building:text() == "") then
								return
							end
							ufo.cmd(string.format("base_selectbuilding %d %d %d",
								tonumber(base_idx:as_string()), building:left(), building:top()))
							return
						end
						-- Build building
						if (not sender:parent():parent():can_build(building:left(), building:top(), build:width(), build:height())) then
							return
						end
						ufo.cmd(string.format("ui_build_building %d %s %d %d",
							tonumber(base_idx:as_string()), build:text(), building:left(), building:top()))
					end,

					on_rightclick = function (sender)
						local build = sender:parent():parent():child("build")
						if (build:text() == "") then
							if (sender:parent():is_building()) then
								ufo.cmd(string.format("building_destroy %s %d %d;",
									sender:root():child("base_idx"):as_string(),
									sender:parent():child("building_id"):left(),
									sender:parent():child("building_id"):top()
								))
							end
							return
						end
						-- Cancel building
						sender:highlight(build:width(), build:height(), false)
						build:set_text("")
					end,

					on_mouseenter = function (sender)
						local build = sender:parent():parent():child("build")
						-- "normal mode" select existing building
						if (build:text() == "") then
							if (sender:parent():is_building()) then
								sender:set_backgroundcolor(0.4, 0.52, 0.5, 0.4)
							end
							return
						end
						-- build new building
						local posX = sender:parent():child("building_id"):left()
						local posY = sender:parent():child("building_id"):top()
						if (not sender:parent():parent():can_build(posX, posY, build:width(), build:height())) then
							return
						end
						sender:highlight(build:width(), build:height(), true)
					end,

					on_mouseleave = function (sender)
						local build = sender:parent():parent():child("build")
						if (build == nil) then
							return
						end
						-- "normal mode" select existing building
						if (build:text() == "") then
							if (sender:parent():is_building()) then
								sender:set_backgroundcolor(0, 0, 0, 0)
							end
							return
						end
						-- build new building
						local posX = sender:parent():child("building_id"):left()
						local posY = sender:parent():child("building_id"):top()
						if (not sender:parent():parent():can_build(posX, posY, build:width(), build:height())) then
							return
						end
						sender:highlight(build:width(), build:height(), false)
					end,
				},

				--[[
				 - @brief Retrns if this building is built uo
				--]]
				is_builtup = function (sender)
					if (not sender:is_building()) then
						return false
					end
					if (sender:child("construction"):text() == "") then
						return true
					end
					return false
				end,
				--[[
				 - @brief Returns if this tile is a building or not
				--]]
				is_building = function (sender)
					if (sender:child("building_id"):text() == "") then
						return false
					else
						return true
					end
				end,
				--[[
				 - @brief Returns if this tile is blocked or not
				--]]
				is_blocked = function (sender)
					if (sender:image() == "base/grid") then
						return false
					else
						return true
					end
				end,
			}, sender)
		end,

		--[[
		 - @brief Get base tile node at a base position
		 - @param x Horizontal position of the tile by tiles
		 - @param y Vertical position of the tile by tiles
		--]]
		get_tile_at = function (sender, x, y)
			local row = tonumber(y)
			local column = tonumber(x)
			if (column < 0 or row < 0) then
				return nil
			end

			-- Multi-tile buildings indexed by the top-left tile, need to backtrack
			for j = 0, y do
				for i = 0, x do
					local tile = sender:child(string.format("tile_%d_%d", column - i, row - j))
					if (tile ~= nil) then
						if (tile:child("building_id"):width() > i) then
							if (tile:child("building_id"):height() > j) then
								return tile
							end
						end
						return nil
					end
				end
			end
			return nil
		end,

		--[[
		 - @brief Determines if this (and surrounding) tile(s) can host a building
		 - @param x Horizontal position of the tile by tiles
		 - @param y Vertical position of the tile by tiles
		 - @param width Building width in tiles
		 - @param height Building height in tiles
		--]]
		can_build = function (sender, x, y, width, height)
			local i, j
			for j = y, y + height - 1 do
				for i = x, x + width - 1 do
					local tile = sender:get_tile_at(i, j)
					if (tile == nil) then
						return false
					end
					if (tile:is_blocked()) then
						return false
					end
				end
			end

			local has_neighbour = false
			for i = x, x + width - 1 do
				local north = sender:get_tile_at(i, y - 1)
				if (north ~= nil and north:is_builtup()) then
					has_neighbour = true
				end
				local south = sender:get_tile_at(i, y + height)
				if (south ~= nil and south:is_builtup()) then
					has_neighbour = true
				end
			end
			if (has_neighbour) then
				return has_neighbour
			end
			for j = y, y + height - 1 do
				local west = sender:get_tile_at(x - 1, j)
				if (west ~= nil and west:is_builtup()) then
					has_neighbour = true
				end
				local east = sender:get_tile_at(x + width, j)
				if (east ~= nil and east:is_builtup()) then
					has_neighbour = true
				end
			end
			return has_neighbour
		end,
	}, root_node)

	local base_building_add = ufox.build({
		name = "base_building_add",
		class = "confunc",

		--[[
		 - @brief Add a building / tile to the base
		 - @param x Horizontal position of the tile by tiles
		 - @param y Vertical position of the tile by tiles
		 - @param width Horizontal size of the tile by tiles
		 - @param height Vertical size of the tile by tiles
		 - @param image Image to show
		 - @param tooltip Tooltip to show on mouse over
		 - @param notice Visible notice string (like under construction)
		--]]
		on_click = function (sender, x, y, width, height, id, name, image, tooltip_text, notice)
			sender:parent():add(tonumber(x), tonumber(y), tonumber(width), tonumber(height), id, name, image, notice)
		end,

	}, map)
	ufo.cmd(string.format("ui_base_fillmap %d;", base_idx))
end

-- header guard
end
