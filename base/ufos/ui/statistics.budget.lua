--!usr/bin/lua

--[[
-- @file
-- @brief Budget report for statistics screen
--]]

--[[
Copyright (C) 2002-2023 UFO: Alien Invasion.

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

require("ufox.lua")

function build_budgetreport (rootNode)
	if (rootNode == nil) then
		return
	end

	if (rootNode:child("budget_report") ~= nil) then
		rootNode:child("budget_report"):delete_node()
	end
	local nationChart = ufox.build({
		name = "budget_report",
		class = "panel",
		pos = {0, 0},
		size = {440, 396},
		backgroundcolor = {0.5, 0.5, 0.5, 0.2},

		{
			name = "expenses_label",
			class = "string",
			text = "_Expenses",
			pos = {0, 0},
			size = {440, 20},
			contentalign = ufo.ALIGN_CC,
			backgroundcolor = {0.527, 0.6, 0.21, 0.2},
		},

		{
			name = "expenses_list",
			class = "panel",
			pos = {0, 20},
			size = {420, 396},
			layout = ufo.LAYOUT_TOP_DOWN_FLOW,
			wheelscrollable = true,

			on_viewchange = function (sender)
				local scrollbar = sender:parent():child("expenses_list_scrollbar")
				if (scrollbar ~= nil) then
					scrollbar:set_fullsize(sender:fullsize())
					scrollbar:set_current(sender:viewpos())
					scrollbar:set_viewsize(sender:viewsize())
				end
			end,

			on_wheel = function (sender)
				local scrollbar = sender:parent():child("expenses_list_scrollbar")
				if (scrollbar ~= nil) then
					scrollbar:set_current(sender:viewpos())
				end
			end,

			clear = function (sender)
				sender:remove_children()
			end,

			add = function (sender, expenseID, expenseName, expenseValue)
				local expenseString = tostring(expenseValue)
				if (expenseString:match("^%d+$")) then
					expenseString = expenseString .. " c"
				end
				local row = ufox.build({
					name = expenseID,
					class = "panel",
					size = {410, 20},
					layout = ufo.LAYOUT_LEFT_RIGHT_FLOW,

					{
						name = "expense_name",
						class = "string",
						text = expenseName,
						size = {300, 20},
						ghost = true,
					},

					{
						name = "expense_value",
						class = "string",
						text = expenseString,
						contentalign = ufo.ALIGN_CR,
						size = {120, 20},
						ghost = true,
					},
				}, sender)
			end,

			add_summary = function (sender)
				local expenses = 0
				local node = sender:first()
				while (node ~= nil) do
					local expstring = node:child("expense_value"):text():gsub(" c", "")
					expenses = expenses + tonumber(expstring)
					node = node:next()
				end
				sender:add("placeholder", "", "")
				sender:add("total", "_Total:", expenses)
				ufox.build({
					name = "expenses_note",
					class = "string",
					text = "_* Unplanned expenses (purchases, sells, etc.) are not included",
					size = {420, 40},
					longlines = ufo.LONGLINES_WRAP,
					font = "f_verysmall",
				}, sender)
			end,
		},

		{
			name = "expenses_list_scrollbar",
			class = "vscrollbar",
			image = "ui/scrollbar_v",
			pos = {420, 20},
			height = 396,
			current = 0,
			viewsize = 10,
			fullsize = 0,
			autoshowscroll = true,

			on_change = function (sender)
				local panel = sender:parent():child("expenses_list")
				if (panel ~= nil) then
					panel:set_viewpos(sender:current())
				end
			end,
		},

		{
			name = "ui_expenses_clear",
			class = "confunc",

			on_click = function (sender)
				sender:parent():child("expenses_list"):clear()
			end,
		},

		{
			name = "ui_expenses_fill",
			class = "confunc",

			on_click = function (sender, expenseID, expenseName, expenseValue)
				sender:parent():child("expenses_list"):add(expenseID, expenseName, expenseValue)
			end,
		},

		{
			name = "ui_expenses_summarize",
			class = "confunc",

			on_click = function (sender)
				sender:parent():child("expenses_list"):add_summary()
			end,
		},
	}, rootNode)
end
