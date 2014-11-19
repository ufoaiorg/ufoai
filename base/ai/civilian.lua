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

local civai = ai

function civai.hidetus ()
	return civai.actor():TU() - 4
end

function civai.flee()
	local flee_pos = civai.positionflee()
	if flee_pos then
		return flee_pos:goto()
	end
	return false
end

function civai.hide (team)
	local hide_pos = civai.positionhide(team)
	if hide_pos then
		return hide_pos:goto()
	end
	return false
end

function civai.herd (targets)
	if #targets > 0 then
		for i = 1, #targets do
			local herd_pos = civai.positionherd(targets[i])
			if herd_pos then
				return herd_pos:goto()
			end
		end
	end
	return false
end

function civai.approach (targets)
	for i = 1, #targets do
		for j = 1, 2 do
			local near_pos = civai.positionapproach(targets[i], civai.hidetus(), j == 1)
			if near_pos then
				return near_pos:goto()
			end
		end
	end
	return false
end

function civai.route (waypoints)
	if #waypoints > 0 then
		for i = 1, #waypoints do
			local target_pos = civai.positionmission(waypoints[i])
			if target_pos then
				if target_pos:goto() then
					civai.setwaypoint(waypoints[i])
				end
			end
		end
		-- Can't get to any waypoints, try to approach the nearest one
		return civai.approach(waypoints)
	end
end

function civai.wander ()
	local next_pos = civai.positionwander("rand", (civai.actor():TU() + 1) / 6)
	if next_pos then
		next_pos:goto()
	end
end

function civai.think_nf ()
	local aliens = civai.see("sight", "alien")
	if #aliens > 0 then
		if not civai.hide(aliens[1]:team()) then
			civai.flee()
		end
	else
		local waypoints = civai.waypoints(25, "path")
		if #waypoints > 0 then
			civai.route(waypoints)
		else
			civai.setwaypoint(nil)
			local phalanx = civai.see("sight", "phalanx")
			if #phalanx > 0 then
				civai.herd(phalanx)
			else
				local civs = civai.see("sight", "civilian")
				if #civs > 0 then
					civai.herd(civs)
				else
					civai.wander()
				end
			end
		end
	end
	aliens = civai.see("sight", "alien")
	if #aliens > 0 then
		-- Some civ models don't have crouching animations
		-- civai.crouch(true)
	end
end

function civai.think ()
	if civai.actor().morale() ~= "normal" then
		civai.flee()
	else
		if not civai.isfighter() then
			civai.think_nf()
		else
			-- We don't currently have fight capable civ teams, so no point in implementing this yet
			civai.print("Warning: no fight capable lua ai available for civilian ", civai.actor())
			civai.think_nf()
		end
	end
end

return ai
