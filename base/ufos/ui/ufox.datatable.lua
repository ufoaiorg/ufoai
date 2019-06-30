--!usr/bin/lua

--[[
-- @file
-- @brief DataTable builder extension
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

-- ufox.build_datatable Header Guard
if (ufox.datatable == nil) then

require("ufox.lua")

ufox.datatable = {
	sortfunction = {
		ascending = function (a, b)
			if (tonumber(a[2]) ~= nil and tonumber(b[2]) ~= nil) then
				return tonumber(a[2]) < tonumber(b[2])
			else
				return a[2] < b[2]
			end
		end,
		descending = function (a, b)
			if (tonumber(a[2]) ~= nil and tonumber(b[2]) ~= nil) then
				return tonumber(b[2]) < tonumber(a[2])
			else
				return b[2] < a[2]
			end
		end,
	},
	sort = function (sender, field, direction)
		local currentSortField = sender:child("sortField")
		local currentSortDirection = sender:child("sortDirection")

		-- Figure out the parameters
		if (field == nil) then
			return
		end

		-- toggle direction unless specified, default to ascending on new fields
		if (direction == nil) then
			if (currentSortField:text() == field:name()) then
				if (currentSortDirection:text() == "ascending") then
					direction = "descending"
				else
					direction = "ascending"
				end
			else
				direction = "ascending"
			end
		end

		local dataSpace = sender:child("data")
		-- copy data to a temp table
		local sortTable = {}
		local row = dataSpace:first()
		while (row ~= nil) do
			local cell = row:child(field:name())
			local value
			if (cell == nil) then
				value = ""
			elseif (cell:child("value") ~= nil) then
				value = cell:child("value"):text()
			else
				value = cell:text()
			end
			table.insert(sortTable, {row:name(), value})
			row = row:next()
		end
		-- sort
		if (direction == "descending") then
			table.sort(sortTable, ufox.datatable.sortfunction.descending)
		else
			table.sort(sortTable, ufox.datatable.sortfunction.ascending)
		end
		-- move the nodes around
		local previous = nil
		for idx, record in ipairs(sortTable) do
			dataSpace:move_node(dataSpace:child(record[1]), previous)
			previous = dataSpace:child(record[1])
		end
		currentSortField:set_text(field:name())
		currentSortDirection:set_text(direction)
	end,
	--[[
	-- @brief Add header fields to the table
	-- @param sender Reference to the dataTable node
	-- @param fields List of (partial) header node definitions:
	--	name	(Required) Name of the table column
	--	width	(Required) Width of the column.
	-- @note These field names and widths are going to be used for data cells too
	--]]
	add_headerFields = function (sender, fields)
		local header = sender:child("header")
		if (header == nil) then
			ufo.print(string.format("ufox.datatable %s has no header\n", sender:name()))
			return
		end
		if (type(fields) ~= "table") then
			ufo.print("ufox.datatable:add_headerFields() was called with incompatible fields parameter\n")
			return
		end

		local left = 0;
		local node = header:first()
		while (node ~= nil) do
			left = left + node:width()
			node = node:next()
		end

		local headerFieldDefaults = {
			class = "string",
			height = 20,
			contentalign = ufo.ALIGN_CC,

			on_click = function (sender)
				sender:parent():parent():sort(sender)
			end,
		}
		for i, field in pairs(fields) do
			-- fill missing data
			for key, value in pairs(headerFieldDefaults) do
				if (field[key] == nil) then
					field[key] = value
				end
			end
			local field = ufox.build(field, header)
			if (field == nil) then
				ufo.print(string.format("ufox.datatable.build encountered error on creating field no. %d\n", i))
				return
			end
			field:set_left(left)
			left = left + field:width()
		end

		return header
	end,

	--[[
	-- @brief Add data cells to a data row
	-- @param sender data row node
	-- @param cellData Table with the data cell
	--]]
	addCell = function (sender, cellData)
		local header = sender:parent():parent():child("header")
		if (header == nil) then
			ufo.print("ufox.datatable has no header\n")
			return
		end

		if (type(cellData) ~= "table") then
			ufo.print(string.format("ufox.datatable %s has no data space\n", sender:name()))
			return
		end

		if (cellData.name == nil or header:child(cellData.name) == nil) then
			ufo.print(string.format("ufox.datatable tried to add data to a non-existent field\n", sender:name()))
			return
		end

		local column = header:child(cellData.name)
		local cellDefaults = {
			class = "string",
			left = column:left(),
			top = 0,
			width = column:width(),
			height = sender:height(),
		}
		-- fill missing data
		for key, value in pairs(cellDefaults) do
			if (cellData[key] == nil) then
				cellData[key] = value
			end
		end
		local cell = ufox.build(cellData, sender)
		return cell
	end,

	--[[
	-- @brief Adds a datarow to the table
	-- @param sender Reference to the data table node
	-- @param rowData Table with row properties:
	--	name	(Required) name (id) of the data row
	-- @param fields Array of tables with dataCell properties
	-- @sa addCell
	--]]
	add_dataRow = function (sender, rowData, fields)
		local header = sender:child("header")
		if (header == nil) then
			ufo.print(string.format("ufox.datatable %s has no header\n", sender:name()))
			return
		end

		local dataSpace = sender:child("data")
		if (dataSpace == nil) then
			ufo.print(string.format("ufox.datatable %s has no data space\n", sender:name()))
			return
		end

		local dataRowDefaults = {
			class = "panel",
			width = sender:width(),
			height = 20,
			addCell = ufox.datatable.addCell,
		}
		for key, value in pairs(dataRowDefaults) do
			if (rowData[key] == nil) then
				rowData[key] = value
			end
		end
		local row = ufox.build(rowData, dataSpace)
		if (row == nil) then
			ufo.print("ufox.datatable:add_dataRow() was called with incompatible parameter\n")
			return
		end

		if (type(fields) == "table") then
			for i, field in ipairs(fields) do
				row:addCell(field)
			end
		end

		return row
	end,

	--[[
	-- @brief Clears the entire datatable (both header and data)
	-- @param sender UI node reference to the data table
	--]]
	clear = function (sender)
		sender:clear_data()
		sender:child("header"):remove_children()
	end,

	--[[
	-- @brief Clears all data from the table
	-- @param sender UI node reference to the data table
	--]]
	clear_data = function (sender)
		sender:child("data"):remove_children()
	end,

	--[[
	-- @brief Function builds a generic datatable
	-- @param rootNode UI node in that the table should be created in
	-- @param tableData Table describes the base node:
	--	name	(Required) Name of the dataTable
	--	pos	(Recommended) Position of the dataTable. Defaults to (0, 0)
	--	size	(Recommended) Size of the dataTable. Fills the entire rootNode by default
	--]]
	build = function (rootNode, tableData, headerProperties)
		if (rootNode == nil or tableData == nil or tableData.name == nil) then
			ufo.print("ufox.datatable.build was called with missing parameters\n")
			return
		end

		local tableDefaults = {
			class = "panel",
			pos = {0, 0},
			size = {rootNode:width(), rootNode:height()},
			backgroundcolor = {0.5, 0.5, 0.5, 0.2},

			{
				name = "sortField",
				class = "string",
				text = "",
				invisible = true,
			},

			{
				name = "sortDirection",
				class = "string",
				text = "",
				invisible = true,
			},

			{
				name = "selected",
				class = "string",
				text = "",
				invisible = true,
			},
		}
		for key, value in pairs(tableDefaults) do
			if (tableData[key] == nil) then
				tableData[key] = value
			end
		end

		local dataTable = ufox.build(tableData, rootNode)
		if (dataTable == nil) then
			ufo.print("ufox.datatable.build failed due to incompatible parameters\n")
			return
		end

		local header = ufox.build({
			name = "header",
			class = "panel",
			pos = {0, 0},
			size = {dataTable:width(), 20},
			backgroundcolor = {0.527, 0.6, 0.21, 0.2},
		}, dataTable)
		-- Allow override properties
		if (headerProperties ~= nil) and (type(headerProperties) == "table") then
			ufox.build_properties(headerProperties, dataTable, header)
		end

		-- add data space
		ufox.build_properties({
			{
				name = "data",
				class = "panel",
				pos = {0, header:height()},
				size = {dataTable:width() - 20, dataTable:height() - header:height()},
				layout = ufo.LAYOUT_TOP_DOWN_FLOW,
				wheelscrollable = true,

				on_viewchange = function (sender)
					local scrollbar = sender:parent():child("data_scrollbar")
					scrollbar:set_fullsize(sender:fullsize())
					scrollbar:set_current(sender:viewpos())
					scrollbar:set_viewsize(sender:viewsize())
				end,

				on_wheel = function (sender)
					local scrollbar = sender:parent():child("data_scrollbar")
					scrollbar:set_current(sender:viewpos())
				end,
			},

			{
				name = "data_scrollbar",
				class = "vscrollbar",
				image = "ui/scrollbar_v",
				pos = {dataTable:width() - 20, 20},
				height = (dataTable:height() - 20),
				current = 0,
				viewsize = 10,
				fullsize = 0,
				autoshowscroll = true,

				on_change = function (sender)
					local panel = sender:parent():child("data")
					panel:set_viewpos(sender:current())
				end,
			},

			-- Add commands
			add_headerFields = ufox.datatable.add_headerFields,
			clear = ufox.datatable.clear,
			clear_data = ufox.datatable.clear_data,
			add_dataRow = ufox.datatable.add_dataRow,
			sort = ufox.datatable.sort,
		}, rootnode, dataTable)

		ufox.build_properties({
		}, rootnode, dataTable)

		return dataTable
	end,
}

-- ufox.datatable Header Guard
end
