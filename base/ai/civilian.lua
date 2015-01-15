--[[
Copyright (C) 2002-2014 UFO: Alien Invasion.

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

local ailc = { }

function ailc.tustouse ()
	return ai.actor():TU() - 3
end

function ailc.flee()
	local flee_pos = ai.positionflee(ailc.tustouse())
	if flee_pos then
		return flee_pos:goto()
	end
	return false
end

function ailc.hide (team)
	local hide_pos = ai.positionhide(team, ailc.tustouse())
	if hide_pos then
		return hide_pos:goto()
	end
	return false
end

function ailc.herd (targets)
	if #targets > 0 then
		for i = 1, #targets do
			local herd_pos = ai.positionherd(targets[i], ailc.tustouse())
			if herd_pos then
				return herd_pos:goto()
			end
		end
	end
	return false
end

function ailc.approach (targets)
	for i = 1, #targets do
		for j = 1, 2 do
			local near_pos = ai.positionapproach(targets[i], ailc.tustouse(), j == 1)
			if near_pos then
				return near_pos:goto()
			end
		end
	end
	return false
end

function ailc.route (waypoints)
	if #waypoints > 0 then
		for i = 1, #waypoints do
			local target_pos = ai.positionmission(waypoints[i], ailc.tustouse())
			if target_pos then
				if target_pos:goto() then
					ai.setwaypoint(waypoints[i])
					return true
				end
			end
		end
		-- Can't get to any waypoints, try to approach the nearest one
		return ailc.approach(waypoints)
	end
end

function ailc.wander ()
	local next_pos = ai.positionwander("rand", (ai.actor():TU() + 1) / 6, ai.actor():pos(), ailc.tustouse())
	if next_pos then
		next_pos:goto()
	end
end

function ailc.think_nf ()
	local aliens = ai.see("sight", "alien")
	if #aliens > 0 then
		if not ailc.hide("alien") then
			ailc.flee()
		end
	else
		local waypoints = ai.waypoints(25, "path")
		if #waypoints > 0 then
			ailc.route(waypoints)
		else
			ai.setwaypoint(nil)
			local phalanx = ai.see("sight", "phalanx")
			if #phalanx > 0 then
				ailc.herd(phalanx)
			else
				local civs = ai.see("sight", "civilian")
				if #civs > 0 then
					ailc.herd(civs)
				else
					ailc.wander()
				end
			end
		end
	end
	aliens = ai.see("sight", "alien")
	if #aliens > 0 then
		-- Some civ models don't have crouching animations
		-- ailc.crouch(true)
	end
end

function ailc.think ()
	if ai.actor():morale() ~= "normal" then
		if not ailc.hide("~civilian") then
			ailc.flee()
		end
	else
		if not ai.isfighter() then
			ailc.think_nf()
		else
			-- We don't currently have fight capable civ teams, so no point in implementing this yet
			ai.print("Warning: no fight capable lua ai available for civilian ", ailc.actor())
			ailc.think_nf()
		end
	end
end

--[[
	Team think function, just run the normal think function, no idea what team tactics would be for civilians
--]]
function ailc.team_think ()
	-- Round just started set up.
	if ailc.squed == nil then
		ailc.squad = ai.squad()
		ailc.actor = 1
	-- Run next actor
	else
		ailc.actor = ailc.actor + 1
	end

	-- Done with this round.
	if ailc.actor > #ailc.squad then
		ailc.squad = nil
		return false
	end

	-- Run the think function for this actor
	ai.select(ailc.squad[ailc.actor])
	if not ai:actor():isdead() then
		ailc.think()
	end

	return true
end

return ailc
