--!usr/bin/lua

--[[
-- @file
-- @brief UFO recovery sell to nations component
--]]

--[[
Copyright (C) 2002-2024 UFO: Alien Invasion.

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
require("ufox.datatable.lua")

if (uforecovery ~= nil) then
	uforecovery.sell = {
		create = function (sender, nodeName, rootNode)
			local sell = ufox.build({
				name = nodeName,
				class = "panel",
				pos = {0, 35},
				size = {rootNode:width(), rootNode:height() - 35},

				{
					name = "sellcomment",
					class = "string",
					pos = {0, 5},
					size = {rootNode:width(), 20},
					text = "_We received the following offers for the UFO:"
				},

				{
					name = "sellbutton",
					class = "MainMenu3Btn",
					text = "_Sell",
					pos = {rootNode:width() - 260, rootNode:height() - 75},
					size = {250, 30},
					disabled = true,
					backgroundcolor = {0.38, 0.48, 0.36, 1},

					on_click = function (sender)
						local selection = sender:parent():child("sellpanel"):child("selected"):text()
						local ufoId = sender:root():child("ufoId"):text()
						local price = sender:parent():child("sellpanel"):child("data"):child(selection):child("price"):child("value"):text()
						ufo.cmd(string.format("cp_uforecovery_sell_start \"%s\" %s %s;ui_pop;", ufoId, selection, price))
					end,
				},
			}, rootNode)

			local dataTable = ufox.datatable.build(sell, {
				name = "sellpanel",
				pos = {0, 35},
				size = {rootNode:width(), rootNode:height() - 120},
			})

			dataTable:add_headerFields({
				{ name = "flag",        width =  30, ghost = true, },
				{ name = "nation_name", width = 300, text = "_Nation", },
				{ name = "price",       width = 110, text = "_Price",   contentalign = ufo.ALIGN_CR, },
				{ name = "happiness",   width = 145, text = "_Happiness", contentalign = ufo.ALIGN_CR, },
			})

			local callback = sell:child("ui_uforecovery_nations")
			if (callback ~= nil) then
				calback:delete_node()
			end
			callback = ufox.build({
				name = "ui_uforecovery_nations",
				class = "confunc",

				on_click = function (sender, nationID, nationName, price, happinessString, happinessValue)
					local dataTable = sender:parent():child("sellpanel")
					dataTable:add_dataRow({
						name = nationID,

						select = function (sender)
							local cell = sender:first()
							while (cell ~= nil) do
								if (cell:type() == "string") then
									cell:set_color(1, 1, 1, 1)
								end
								cell = cell:next()
							end
						end,

						deselect = function (sender)
							local cell = sender:first()
							while (cell ~= nil) do
								if (cell:type() == "string") then
									cell:set_color(1, 1, 1, 0.5)
								end
								cell = cell:next()
							end
						end,

						on_click = function (sender)
							local selection = sender:parent():parent():child("selected")

							local selectedRow = sender:parent():child(selection:text())
							if (selectedRow ~= nil) then
								selectedRow:deselect()
							end

							selection:set_text(sender:name())
							sender:select()

							local sellButton = sender:parent():parent():parent():child("sellbutton")
							if (sellButton ~= nil) then
								sellButton:set_disabled(false)
							end
						end,
					}, {
						{
							name = "flag",
							source = "nations/" .. nationID,
							class = "image",
							keepratio = true,
							ghost = true,
						},

						{
							name = "nation_name",
							text = nationName,
							color = {1, 1, 1, 0.5},
							ghost = true,
						},

						{
							name = "price",
							text = string.format("_%d c", price),
							contentalign = ufo.ALIGN_CR,
							color = {1, 1, 1, 0.5},
							ghost = true,

							{
								name = "value",
								class = "string",
								text = price,
								invisible = true,
							},
						},

						{
							name = "happiness",
							text = happinessString,
							contentalign = ufo.ALIGN_CR,
							color = {1, 1, 1, 0.5},
							ghost = true,

							{
								name = "value",
								class = "string",
								text = happinessValue,
								invisible = true,
							},
						},
					})
				end,
			}, sell)

			local ufoType = sender:root():child("ufoType"):text()
			ufo.cmd(string.format("cp_uforecovery_sell_init %s;", ufoType))
		end,

		register = function (rootNode, tabset)
			local button = ufox.build({
				name = "sell",
				class = "MainMenuTab",
				text = "_Sell UFO",
				width = 135,

				create = uforecovery.sell.create,

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
end
