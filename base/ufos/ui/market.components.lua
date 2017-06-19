--!usr/bin/lua

require("ufox.lua")

-- Market category label
do
	local MarketCategory = ufox.build_component({
		name = "MarketCategory",
		class = "panel",
		size = {713, 28},
		backgroundcolor = {0.56, 0.81, 0.76, 0.3},

		on_mouseenter = function (sender)
			sender:set_backgroundcolor(0.56, 0.81, 0.76, 0.4)
		end,

		on_mouseleave = function (sender)
			sender:set_backgroundcolor(0.56, 0.81, 0.76, 0.3)
		end,

		{
			name = "name",
			class = "string",
			pos = {30, 0},
			size = {400, 28},
			color = {0.56, 0.81, 0.76, 1},
			contentalign = ufo.ALIGN_CL,
			ghost = true,
		},
	})
end

-- Market category list
do
	local MarketList = ufox.build_component({
		name = "MarketList",
		class = "panel",
		size = {713, 0},
		layout = ufo.LAYOUT_TOP_DOWN_FLOW,
		layoutmargin = 2,
	})
end

-- Market item
do
	local MarketItem = ufox.build_component({
		name = "MarketItem",
		class = "panel",
		pos = {0, 0},
		size = {713, 28},
		backgroundcolor = {0.56, 0.81, 0.76, 0.1},

		on_mouseenter = function (sender)
			sender:set_backgroundcolor(0.56, 0.81, 0.76, 0.15)
		end,

		on_mouseleave = function (sender)
			sender:set_backgroundcolor(0.56, 0.81, 0.76, 0.1)
		end,

		on_click = function (sender)
			ufo.cmd(string.format("ui_market_select %q;", sender:child("id"):as_string()))
		end,

		{
			name = "id",
			class = "data",
		},

		{
			name = "rate",
			class = "data",
			value = 1,
		},

		-- UFOPaedia link for each item
		{
			name = "ufopaedia",
			class = "button",
			icon = "icons/windowinfo",
			tooltip = "_View UFOPaedia entry",
			pos = {5, 5},
			size = {18, 18},

			on_click = function (sender)
				ufo.cmd(string.format("ui_market_showinfo %q;", sender:parent():child("id"):as_string()))
				ufo.cmd("ui_market_openpedia;")
			end,
		},

		{
			name = "name",
			class = "string",
			pos = {30, 0},
			size = {290, 28},
			color = {0.56, 0.81, 0.76, 0.7},
			contentalign = ufo.ALIGN_CL,
			ghost = true,
		},

		{
			name = "base",
			class = "string",
			pos = {320, 0},
			size = {80, 28},
			color = {0.56, 0.81, 0.76, 1},
			contentalign = ufo.ALIGN_CR,
			ghost = true,
		},

		-- Buy items
		{
			name = "buy",
			class = "spinner",
			pos = {406, 6},
			size = {74, 16},
			topicon = "icons/arrowtext_lft",
			mode = ufo.SPINNER_ONLY_INCREASE,
			delta = 1,
			shiftmultiplier = 10,

			on_mouseenter = function (sender)
				sender:set_topicon("icons/arrowtext_lft0")
			end,

			on_mouseleave = function (sender)
				sender:set_topicon("icons/arrowtext_lft")
			end,

			activate = function (sender)
				-- ufo.print(string.format("Change: min: %i current: %i max: %i delta: %i lastdiff: %i\n", sender:min(), sender:value(), sender:max(), sender:delta(), sender:lastdiff()))
				ufo.cmd(string.format("ui_market_buy \"%s\" %s;", sender:parent():child("id"):as_string(), tostring(sender:lastdiff())))
				ufo.cmd(string.format("ui_market_fill %s;", ufo.findvar("ui_market_category"):as_string()))
				ufo.cmd(string.format("ui_market_select \"%s\";", sender:parent():child("id"):as_string()))
			end,
		},

		{
			name = "buy_price",
			class = "string",
			pos = {406, 6},
			size = {74, 16},
			color = {0, 0, 0, 1},
			font = "f_verysmall_bold",
			contentalign = ufo.ALIGN_CR,
			ghost = true,
		},

		{
			name = "autosell",
			class = "checkbox",
			tooltip = "_Lock current stock level",
			value = 0,
			iconchecked = "icons/windowlock",
			iconunchecked = "icons/windowlock_light",
			iconunknown = "icons/windowlock_light",
			pos = {484, 5},
			size = {18, 18},
			invisible = false,

			on_mouseenter = function (sender)
				sender:set_iconunchecked("icons/windowlock")
			end,

			on_mouseleave = function (sender)
				sender:set_iconunchecked("icons/windowlock_light")
			end,

			on_click = function (sender)
				ufo.cmd(string.format("ui_market_setautosell \"%s\" %s;", sender:parent():child("id"):as_string(), tostring(sender:as_integer())))
			end,
		},

		-- Sell items
		{
			name = "sell",
			class = "spinner",
			pos = {508, 6},
			size = {74, 16},
			topicon = "icons/arrowtext_rgt",
			mode = ufo.SPINNER_ONLY_DECREASE,
			delta = 1,
			shiftmultiplier = 10,

			on_mouseenter = function (sender)
				sender:set_topicon("icons/arrowtext_rgt0")
			end,

			on_mouseleave = function (sender)
				sender:set_topicon("icons/arrowtext_rgt")
			end,

			activate = function (sender)
				-- ufo.print(string.format("Change: min: %i current: %i max: %i delta: %i lastdiff: %i\n", sender:min(), sender:value(), sender:max(), sender:delta(), sender:lastdiff()))
				ufo.cmd(string.format("ui_market_buy \"%s\" %s;", sender:parent():child("id"):as_string(), tostring(sender:lastdiff())))
				ufo.cmd(string.format("ui_market_fill %s;", ufo.findvar("ui_market_category"):as_string()))
				ufo.cmd(string.format("ui_market_select \"%s\";", sender:parent():child("id"):as_string()))
			end,
		},

		{
			name = "sell_price",
			class = "string",
			pos = {508, 6},
			size = {74, 16},
			color = {0, 0, 0, 1},
			font = "f_verysmall_bold",
			contentalign = ufo.ALIGN_CL,
			ghost = true,
		},

		-- Count of items in the market
		{
			name = "market",
			class = "string",
			pos = {586, 0},
			size = {100, 28},
			color = {0.56, 0.81, 0.76, 1},
			contentalign = ufo.ALIGN_CL,
			ghost = true,
		},
	})
end
