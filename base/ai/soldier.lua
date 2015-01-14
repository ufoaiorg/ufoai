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

local ails = { }

ails.params = {	vis = "sight", ord = "dist", pos = "fastest", move = "rand", prio = {"~civilian"} }

function ails.tustouse ()
	return ai.actor():TU() - 4
end

function ails.ismelee()
	local right, left = ai.weapontype()
	return (right == "melee" and (left == "melee" or left == "none")) or (right == "none" and left == "melee")
end

function ails.flee ()
	local flee_pos = ai.positionflee(ai.actor():TU() - 3)
	if flee_pos then
		return flee_pos:goto()
	end
	return false
end

function ails.hide ()
	local hide_pos = ai.positionhide("~phalanx", ails.tustouse())
	if hide_pos then
		return hide_pos:goto()
	end
	return false
end

function ails.herd ()
	local team = ai.see("all", "phalanx", "path")
	if #team > 0 then
		for i = 1, #team do
			local herd_pos = ai.positionherd(team[i], ails.tustouse())
			if herd_pos then
				return herd_pos:goto()
			end
		end
	end
	return false
end

function ails.shield ()
	local civs = ai.see(ails.param.vis, "civilian", "HP")
	if #civs > 0 then
		for i = 1, #civs do
			local herd_pos = ai.positionherd(civs[i], ails.tustouse(), true)
			if herd_pos then
				return herd_pos:goto()
			end
		end
	end
	return false
end

function ails.approach (targets)
	for i = 1, #targets do
		local near_pos
		for j = 1, 2 do
			if targets[i].pos then
				near_pos = ai.positionapproach(targets[i]:pos(), ails.tustouse(), j == 1)
			else
				near_pos = ai.positionapproach(targets[i], ails.tustouse(), j == 1)
			end
			if near_pos then
				break
			end
		end
		if near_pos then
			near_pos:goto()
			return targets[i]
		end
	end
	return nil
end

function ails.search ()
	-- First check if we have a mission target
	local targets = ai.missiontargets("all", "phalanx", "path")
	if #targets < 1 then
		-- Check if we can block an enemy target
		for i = 1, #ails.param.prio do
			targets = ai.missiontargets("all", ails.param.prio[i], "path")
			if #targets > 0 then
				break
			end
		end
	end

	local found
	if #targets > 0 then
		for i = 1, #targets do
			local target_pos = ai.positionmission(targets[i], ails.tustouse())
			if target_pos then
				return target_pos:goto()
			end
		end
		-- Can't get to any mission target, try to approach the nearest one
		found = ails.approach(targets)
	end

	-- Try to protect the civilians
	if not found then
		found = ails.shield()
	end

	-- Nothing found, wander around
	if not found then
		if ails.param.move == "herd" then
			ails.herd()
		elseif ails.param.move == "hide" then
			ails.hide()
		else
			local search_rad = (ails.tustouse() - ai.tusforshooting() + 1) / 2
			if search_rad < 1 then
				search_rad =  ails.tustouse() + 1 / 2
			end
			local next_pos = ai.positionwander(ails.param.move, search_rad, ai.actor():pos(), ails.tustouse())
			if next_pos then
				next_pos:goto()
			end
		end
	end
end

function ails.searchweapon ()
	local weapons = ai.findweapons()
	if #weapons > 0 then
		weapons[1]:goto()
		return ai.grabweapon()
	end
	return false
end

function ails.readyweapon ()
	local has_right, has_left = ai.actor():isarmed()
	local right_ammo, left_ammo = ai.roundsleft()
	if not right_ammo and not left_ammo then
		if has_right then
			ai.reload("right")
		end
		right_ammo, left_ammo = ai.roundsleft()
		if has_left and not right_ammo then
			ai.reload("left")
		end
	end

	right_ammo, left_ammo = ai.roundsleft()
	return right_ammo or left_ammo or ai.grabweapon()
end

-- Shoot (from current position) the first suitable target in the table
function ails.shoot (targets)
	local min_group = 3 -- Min enemy group for grenade throw

	for i = 1, #targets do
		local target = targets[i]
		-- Throw a grenade if enough enemies are grouped
		local shot
		-- Be nice in low difficulties and don't throw grenades
		if ai.difficulty() >= 0 then
			 shot = target:throwgrenade(min_group, ails.tustouse())
		end
		-- Shoot
		if not target:isdead() then
			shot = target:shoot(ails.tustouse()) or shot
		end
		if shot then
			return target
		end
	end
	return nil
end

-- Attack the first suitable target in the table - move to position and shoot
function ails.attack (targets)
	for i = 1, #targets do
		-- Get a shoot position
		local shoot_pos = ai.positionshoot(targets[i], ails.param.pos, ails.tustouse())
		if shoot_pos then
			-- Move until target in sight
			shoot_pos:goto()

			local target = ails.shoot{targets[i]}
			if target then
				return target
			end
		end
	end
	return nil
end

-- Engage the first suitable target in the table - approaching if currently out of range
function ails.engage(targets)
	local target
	for i = 1, #targets do
		target = ails.attack{targets[i]}
		if target then
			break
		end
	end
	if not target then
		ai.reactionfire("enable")
		target = ails.approach(targets)
	end
	return target
end

function ails.findtargets(vision, team, order)
	local targets = { }
	local seen = ai.see(vision, team, order)
	if #seen > 0 then
		for i = 1, #seen do
			if seen[i]:isvalidtarget() then
				targets[#targets + 1] = seen[i]
			end
		end
	end
	return targets
end

-- Short term reactionary phase
function ails.phase_one ()
	if ai.isfighter() and ai.actor():morale() ~= "cower" and ails.readyweapon() then
		-- If we don't have enough TUs for shooting try disabling reaction fire to get more available TUs
		if ai.tusforshooting() > ails.tustouse() then
			ai.reactionfire("disable")
		end
		local targets = ails.findtargets(ails.param.vis, "~civilian", "dist")
		while #targets > 0 do
			if ai.actor():isinjured() then
				ails.target = ails.shoot(targets)
			else
				ails.target = ails.attack(targets)
			end
			-- We died attacking or cannot attack
			if ai.actor():isdead() or not ails.target or ai.tusforshooting() > ails.tustouse() then
				return
			end
			targets = ails.findtargets(ails.param.vis, "~civilian", "dist")
		end
	end
end

-- Longer term actions
function ails.phase_two ()
	if not ai.actor():isdead() and ai.isfighter() and ai.actor():morale() ~= "cower" then
		if not ails.readyweapon() then
			if ails.searchweapon() then
				ails.phase_two()
			end
		elseif not ai.actor():isinjured() and ails.tustouse() >= ai.tusforshooting() then
			local done
			for i = 1, #ails.param.prio do
				local targets = ails.findtargets(ails.param.vis, ails.param.prio[i], ails.param.ord)
				while #targets > 0 do
					ails.target = ails.engage(targets)
					-- Did we die while attacking?
					if ai:actor():isdead() then
						return
					end
					-- No target in sight or we failed to kill it
					if not ails.target or not ails.target:isdead() then
						done = true
						break
					end
					targets = ails.findtargets(ails.param.vis, ails.param.prio[i], ails.param.ord)
				end
				if done then
					break
				end
			end
			if not ails.target then
				ai.reactionfire("disable")
				ai.crouch(false)
				ails.search()
			end
		end
	end
end

-- Round end actions
function ails.phase_three ()
	if not ai.actor():isdead() then
		for i = 1, #ails.param.prio do
			local targets = ails.findtargets(ails.param.vis, ails.param.prio[i], ails.param.ord)
			if #targets > 0 and ai.tusforshooting() <= ails.tustouse() then
				ails.target = ails.shoot(targets) or ails.target
				if ai.actor():isdead() then
					return
				end
			end
		end

		local hid
		if ai.hideneeded() then
			hid = ails.hide()
		elseif ai.actor():isinjured() then
			hid = ails.herd() or ails.hide()
		end
		if not hid and ai.actor():morale() == "cower" then
			ails.flee()
		end

		if ails.target then
			ails.target:pos():face()
		end
		if not ai.reactionfire() and ai.tusforshooting() <= ai.actor():TU() then
			ai.reactionfire("enable")
		end
		ai.crouch(true)
	end
end

function ails.prethink ()
	local morale = ai.actor():morale()
	if morale == "panic" then
		if not ails.hide() then
			ails.flee()
		end
		return
	end

	-- copy the default params for this actor
	ails.param = { vis = ails.params.vis, ord = ails.params.ord, pos = ails.params.pos, move = ails.params.move, prio = ails.params.prio }
	-- adjust for morale
	if morale == "rage" or morale == "insane" then
		ails.param.pos = "nearest"
		ails.param.move = "rand"
		if morale == "insane" then
			ails.params.prio = {"all"}
		end
	end
end

function ails.think ()
	ails.target = nil
	ails.phase_one()
	ails.phase_two()
	ails.phase_three()
end

--[[
	Team AI example
	No actual team tactics, just run the AI by phases, first short term actions for everybody followed by
	longer term actions for all and close with round end actions for everybody
--]]

ails.phase = 0

function ails.team_think ()
	-- Turn just started set things up.
	if ails.phase < 1 then
		ails.squad = ai.squad()
		ails.actor = 1
		ails.phase = 1
		ails.targets = {}
	-- Run next actor
	else
		ails.actor = ails.actor + 1
	end

	-- We are done with this phase advance to next one.
	if ails.actor > #ails.squad then
		ails.phase = ails.phase + 1
		ails.actor = 1
	end

	-- We are done thinking for the turn.
	if #ails.squad < 1 or ails.phase > 3 then
		ails.phase = 0
		return false
	end

	ai.select(ails.squad[ails.actor])
	if not ai.actor():isdead() then
		ails.prethink()
		ails.target = ails.targets[ails.actor]
		if ails.phase == 1 then
			ails.phase_one()
		elseif ails.phase == 2 then
			ails.phase_two()
		else
			ails.phase_three()
		end
		ails.targets[ails.actor] = ails.target
	end

	-- Come back again, we are not done with this turn yet
	return true
end

return ails
