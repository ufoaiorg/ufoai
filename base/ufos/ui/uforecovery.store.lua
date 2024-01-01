--!usr/bin/lua

--[[
-- @file
-- @brief UFO recovery store in UFOYard component
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

if (uforecovery ~= nil) then
	uforecovery.store = {
		create = function (sender, nodeName, rootNode)
			local sell = ufox.build({
				name = nodeName,
				class = "panel",
				pos = {0, 35},
				size = {rootNode:width(), rootNode:height() - 35},

				{
					name = "storecomment",
					class = "string",
					pos = {0, 5},
					size = {rootNode:width(), 20},
					text = "The following UFO Yards have free capacity to host this vessel:",
				},

				{
					name = "storebutton",
					class = "MainMenu3Btn",
					text = "_Store",
					pos = {rootNode:width() - 260, rootNode:height() - 75},
					size = {250, 30},
					disabled = true,
					backgroundcolor = {0.38, 0.48, 0.36, 1},

					on_click = function (sender)
						local selection = sender:parent():child("storepanel"):child("selected"):text():gsub("idx_", "")
						local ufoType = sender:root():child("ufoType"):text()
						local ufoDamage = sender:root():child("ufoDamage"):text()
						ufo.cmd(string.format("cp_uforecovery_store_start %s %s %s;ui_pop;", ufoType, ufoDamage, selection))
					end,
				},
			}, rootNode)

			local dataTable = ufox.datatable.build(sell, {
				name = "storepanel",
				pos = {0, 35},
				size = {rootNode:width(), rootNode:height() - 120},
			})

			dataTable:add_headerFields({
				{ name = "name",  width = 400, text = "_UFO Yard", },
				{ name = "space", width = 190, text = "_Space",   contentalign = ufo.ALIGN_CR, },
			})

			local callback = sell:child("ui_uforecovery_ufoyards")
			if (callback ~= nil) then
				calback:delete_node()
			end
			callback = ufox.build({
				name = "ui_uforecovery_ufoyards",
				class = "confunc",

				on_click = function (sender, installationIdx, InstallationName, freeCapacity, fullCapacity)
					local dataTable = sender:parent():child("storepanel")
					dataTable:add_dataRow({
						name = "idx_" .. installationIdx,

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

							local sellButton = sender:parent():parent():parent():child("storebutton")
							if (sellButton ~= nil) then
								sellButton:set_disabled(false)
							end
						end,
					}, {
						{
							name = "name",
							text = InstallationName,
							color = {1, 1, 1, 0.5},
							ghost = true,
						},

						{
							name = "space",
							text = string.format("%s / %s", freeCapacity, fullCapacity),
							contentalign = ufo.ALIGN_CR,
							color = {1, 1, 1, 0.5},
							ghost = true,

							{
								name = "value",
								class = "string",
								text = freeCapacity,
								invisible = true,
							},
						},
					})
				end,
			}, sell)

			ufo.cmd(string.format("cp_uforecovery_store_init;"))
		end,

		register = function (rootNode, tabset)
			local button = ufox.build({
				name = "store",
				class = "MainMenuTab",
				text = "_Store UFO",
				width = 135,

				create = uforecovery.store.create,

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
