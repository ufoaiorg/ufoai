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

--[[
	AI module methods, these work on the currently moving AI actor (Unless noted otherwise parameters are optional):
		print (...) -- Works more or less like Lua's builtin print.

		squad () -- Returns a table of actors of the current AI player. (Only works if the lua AI is in team mode)

		select (actor) -- Manually select the current AI actor. (Only works if the lua AI is in team mode)
			actor -- The actor to select as the moving AI actor.

		see (vision_type, team, sort_order) -- Returns a table of actors (userdatas) the current AI actor can see.
			vision_type -- What to see with, valid values:
				"all":		See everything (cheat vision), this is the default
				"sight":	Normal eyesight for the actor.
				"team":		See what all team member see.
				"extra":	Extrasensory perception currently works as IR vision for the current actor.
			team -- Which team's members to include in the result:
				"all":		Include all teams (default)
				"phalanx":	Single player player's team
				"civilian":	Self explanatory
				"alien":	Self explanatory
				Note: Prefixing the team name with '-' or '~' will inverse the team rules (means: members *not* from the given team), *do not* use with "all"
			sort_order -- Order to sort the result in (ascending order):
				"dist":		Linear distance to seen actor (default)
				"path":		Pathing distance to seen actor.
				"HP":		HP of the seen actor.

		crouch (state) -- Check if the AI actor is crouching (returns a boolean value) and optionally change stance.
			state -- Ask the AI actor to asume an specific stance:
				true:		Crouch.
				false:		Stand.

		reactionfire (state) -- Check if Reaction Fire is enabled (returns a boolean value) and optionally change it
			state -- Ask to change the RF state:
				"disable":	Disable reaction fire.
				Any other string: Enable reaction fire.

		isfighter () -- Chack if the current AI actor is capable of fighting (can use weapons or has an onlyweapon)

		weapontype () -- Return the types of the weapons the actor is holding (two strings -- right and left hand)

		grabweapon () -- Try to get a working weapon from the AI actor's inventory or the floor (to the right hand)

		findweapons (full_search) -- Returns a table of the positions (userdatas) of nearby usable weapons on the floor
			full_search -- If true include unreachable weapons

		roundsleft () -- Return the number rounds left in each hand's weapon (two values, for right and lef hand respectively) or nil if the weapon is unloaded (zero is returned only for weapons that _don't_ need any ammo)

		canreload () -- Check if weapons can be reloaded (returns two boolean values -- right and left hand)

		reload (hand) -- Try to reload a weapon
			hand -- Which hand's weapon to reload, valid values: "right" (default) and "left"

		positionshoot (target, position_type, tus) -- Returns a position (pos3 userdata) from which the target actor can be shot or flase in none found.
			target -- *Required* Actor (userdata) to shoot at.
			position_type -- Strategy to find the shooting position:
				"fastest":	(Default) Less pathing cost to get to.
				"nearest":	Closest to the target.
				"farthest":	Farhest from the target.
			tus -- Max number of TUs to use for moving + shooting (defaults to use all tus).

		postionhide (team, tus) -- Returns a position (pos3 userdata) for the AI actor to hide in or false if none found.
			team -- Team to hide from, valid values: "phalanx", "civilian" and "alien", defaults to "alien" if AI is civilian and "all but our own team" otherwise.
					Note: Prefixing the team name with '-' or '~' will inverse the team rules (means: members *not* from the given team)
			tus -- Max number of TUs to use for moving (defaults to use all tus).

		positionherd (target, tus, inverse) -- Returns a position (pos3 userdata) from where target can be used as a meatshield or false if none found.
			target -- *Required* Actor (userdata) to hide behind.
			tus -- Max number of TUs to use for moving (defaults to use all tus).
			inverse -- If true try to shield _target_ instead of using it as shield (defaults to false).

		positionapproach (target, tus, hide) -- Returns a position closer to target or false if none found (Only considers positions along the fastest path to the target)
			target -- *Required* Actor (userdata) to approach to.
			tus	-- Max number of to use to get there (defaults to use all tus).
			hide -- If true the position returned must not be visible by the target actor's team.

		missiontargets (vision, team, sort_order) -- Return a table of positions (userdatas) of visible mission targets.
			vision -- What to see with. Like for see() but only accepts "all" (default), "sight" and "extra"
			team -- Which team mission targets to get. Accepts same values as see() (defaults to "all")
			sort_order -- In which order to sort the targets. LIke for see() but only allows "dist" (default) and "path"

		positionmission (position) -- Returns a position in the area of the given target position, it is meant to have the AI defend mission targets or move to waypoints without all AI actors trying to move into the exact same position.
			position -- Position (pos3 userdata) to move close to.

		waypoints (distance, sort_order) -- Return a table of positions (userdatas) of the next waypoints for the current AI actor (Note: currently only civilian waypoints exist)
			distance -- Waypoints closer than this won't be considered (maptiles)
			sort_order -- Order to sort the waypoints in. Same as see() but only "dist" (default) and "path" are available

		setwaypoint (position) -- Sets the current AI actor waypoint
			position -- Position of the waypoint to set as current, if omited clear the current waypoint (to restart the search)

		positionwander (strategy, origin, radius) -- Returns a position (userdata) to wander to within the given area.
			strategy -- Which strategy to use for wandering.
				"rand":		Choose a random position within the given area (default)
				"CW": 		Try to move on a clockwise circle around the given area
				"CCW":		Try to move counter-clockwise around the given area.
			origin -- Center of the area to wander about, defaults to current actor position.
			radius -- Radius -in maptiles- of the area to wander about, defaults to current actor TUs / 2
			tus -- Max number of TUs to use for moving (defaults to use all tus).

		positionflee (tus) -- Returns a position (userdata) where to flee.
			tus -- Max number of TUs to use for moving (defaults to use all tus).

		difficulty () -- Returns the current difficulty of the batlescape from -4 (easiest) to 4 (hardest)

		actor () -- Returns the currently mocving AI actor (userdata)

		class () -- Returns the current AI actor's class (LUA AI subtype).

		hideneeded () -- Returns true if the AI actor need hiding (Low morale or exposed position) false otherwise.

		tusforshooting () -- Returns the min TUs the current AI actor needs for shooting its weapon.


	Actor (userdata) metatable methods (Parameters required unless a default is noted)
		pos (actor) -- Returns the given actor's positon (userdata)

		team (actor) -- Returns the given actor's team.

		isvalidtarget (actor) -- Check if the given actor is a valid target for the current AI actor.

		shoot (actor, tus) -- Makes the current AI actor try to shoot the given actor, returns true if shots were fired.
			actor -- Actor to shoot at.
			tus -- Max TUs to use for shooting (defaults to using all available TUs)

		throwgrenade (actor, min_group, tus) -- Makes the current AI actor try to throw a grenade at the given actor, returns true if a grenade was thrown.
			actor -- Actor to throw at.
			min_group -- Min number of actors that must be within the grenades splash radius to throw the grenade (defaults to zero)
			tus -- Max TUs to use for shooting (defaults to using all available TUs)

		TU (actor) -- Return the actor's current Time Units.

		HP (actor) -- Return the actor's current Hit Points.

		morale (actor) -- Return the actor's current morale state ("normal", "panic", "insane", "rage" or "cower")

		isinjured (actor) -- Check if the AI actor is injured (wounded or HP < maxHP * 0.5), returns a boolean value

		isdead (actor) -- Check if the current AI actor is dead, returns a boolean value

		isarmed (actor) -- Check if AI actor has weapons, returns two booleans one for each hand.

	Position (aka pos3 -- userdata) metatable methods (Parameters required unless a default is noted)
		goto (position) -- Makes the current AI actor move to the given position, returns true if the actor reached the target positon.

		face (position) -- Makes the current AI actor try to turn to the direction of the given postion.

		distance (position, type) -- Returns the distance to the given position.
			type -- "dist" (default -- linear distance in map units) or "path" (pathing distance)
--]]

local aila = { }

aila.params = {
	taman =	{ vis = "team", ord = "dist", pos = "fastest", move = "CW", prio = {"~civilian", "civilian"} },
	shevaar = { vis = "extra", ord = "path", pos = "farthest", move = "CCW", prio = {"~civilian", "civilian"} },
	ortnok = { vis = "extra", ord = "HP", pos = "nearest", move = "rand", prio = {"~civilian", "civilian"} },
	bloodspider = { vis = "team", ord = "path", pos = "nearest", move = "hide", prio = {"civilian", "~civilian"} },
	bloodspider_adv = { vis = "team", ord = "path", pos = "nearest", move = "hide", prio = {"~alien"} },
	hovernet = { vis = "team", ord = "dist", pos = "fastest", move = "herd", prio = {"~civilian", "civilian"} },
	hovernet_adv = { vis = "team", ord = "dist", pos = "farthest", move = "herd", prio = {"~civilian", "civilian"} },
	default = { vis = "sight", ord = "dist", pos = "fastest", move = "rand", prio = {"~alien"} }
}

function aila.tustouse ()
	return ai.actor():TU() - 4
end

function aila.ismelee()
	local right, left = ai.weapontype()
	return (right == "melee" and (left == "melee" or left == "none")) or (right == "none" and left == "melee")
end

function aila.flee ()
	local flee_pos = ai.positionflee(ai.actor():TU() - 3)
	if flee_pos then
		return flee_pos:goto()
	end
	return false
end

function aila.hide ()
	local hide_pos = ai.positionhide("~alien", aila.tustouse())
	if hide_pos then
		return hide_pos:goto()
	end
	return false
end

function aila.herd ()
	local aliens = ai.see("all", "alien", "path")
	if #aliens > 0 then
		for i = 1, #aliens do
			local herd_pos = ai.positionherd(aliens[i], aila.tustouse())
			if herd_pos then
				return herd_pos:goto()
			end
		end
	end
	return false
end

function aila.approach (targets)
	for i = 1, #targets do
		local near_pos
		for j = 1, 2 do
			if targets[i].pos then
				near_pos = ai.positionapproach(targets[i]:pos(), aila.tustouse(), j == 1)
			else
				near_pos = ai.positionapproach(targets[i], aila.tustouse(), j == 1)
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

function aila.search ()
	-- First check if we have a mission target
	local targets = ai.missiontargets("all", "alien", "path")
	if #targets < 1 then
		-- Check if we can block an enemy target
		for i = 1, #aila.param.prio do
			targets = ai.missiontargets("all", aila.param.prio[i], "path")
			if #targets > 0 then
				break
			end
		end
	end

	local found
	if #targets > 0 then
		for i = 1, #targets do
			local target_pos = ai.positionmission(targets[i], aila.tustouse())
			if target_pos then
				return target_pos:goto()
			end
		end
		-- Can't get to any mission target, try to approach the nearest one
		found = aila.approach(targets)
	end

	-- Nothing found, wander around
	if not found then
		if aila.param.move == "herd" then
			aila.herd()
		elseif aila.param.move == "hide" then
			aila.hide()
		else
			local search_rad = (aila.tustouse() - ai.tusforshooting() + 1) / 2
			if search_rad < 1 then
				search_rad =  aila.tustouse() + 1 / 2
			end
			local next_pos = ai.positionwander(aila.param.move, search_rad, ai.actor():pos(), aila.tustouse())
			if next_pos then
				next_pos:goto()
			end
		end
	end
end

function aila.searchweapon ()
	local weapons = ai.findweapons()
	if #weapons > 0 then
		weapons[1]:goto()
		return ai.grabweapon()
	end
	return false
end

function aila.readyweapon ()
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
function aila.shoot (targets)
	local min_group = 3 -- Min enemy group for grenade throw

	for i = 1, #targets do
		local target = targets[i]
		-- Throw a grenade if enough enemies are grouped
		local shot
		-- Be nice in low difficulties and don't throw grenades
		if ai.difficulty() >= 0 then
			 shot = target:throwgrenade(min_group, aila.tustouse())
		end
		-- Shoot
		if not target:isdead() then
			shot = target:shoot(aila.tustouse()) or shot
		end
		if shot then
			return target
		end
	end
	return nil
end

-- Attack the first suitable target in the table - move to position and shoot
function aila.attack (targets)
	for i = 1, #targets do
		-- Get a shoot position
		local shoot_pos = ai.positionshoot(targets[i], aila.param.pos, aila.tustouse())
		if shoot_pos then
			-- Move until target in sight
			shoot_pos:goto()

			local target = aila.shoot{targets[i]}
			if target then
				return target
			end
		end
	end
	return nil
end

-- Engage the first suitable target in the table - approaching if currently out of range
function aila.engage(targets)
	local target
	for i = 1, #targets do
		target = aila.attack{targets[i]}
		if target then
			break
		end
	end
	if not target then
		ai.reactionfire("enable")
		target = aila.approach(targets)
	end
	return target
end

function aila.findtargets(vision, team, order)
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
function aila.phase_one ()
	if ai.isfighter() and ai.actor():morale() ~= "cower" and aila.readyweapon() then
		-- If we don't have enough TUs for shooting try disabling reaction fire to get more available TUs
		if ai.tusforshooting() > aila.tustouse() then
			ai.reactionfire("disable")
		end
		local targets = aila.findtargets(aila.param.vis, "~civilian", "dist")
		while #targets > 0 do
			if ai.actor():isinjured() then
				aila.target = aila.shoot(targets)
			else
				aila.target = aila.attack(targets)
			end
			-- We died attacking or cannot attack
			if ai.actor():isdead() or not aila.target or ai.tusforshooting() > aila.tustouse() then
				return
			end
			targets = aila.findtargets(aila.param.vis, "~civilian", "dist")
		end
	end
end

-- Longer term actions
function aila.phase_two ()
	if not ai.actor():isdead() and ai.isfighter() and ai.actor():morale() ~= "cower" then
		if not aila.readyweapon() then
			if aila.searchweapon() then
				aila.phase_two()
			end
		elseif not ai.actor():isinjured() and aila.tustouse() >= ai.tusforshooting() then
			local done
			for i = 1, #aila.param.prio do
				local targets = aila.findtargets(aila.param.vis, aila.param.prio[i], aila.param.ord)
				while #targets > 0 do
					aila.target = aila.engage(targets)
					-- Did we die while attacking?
					if ai:actor():isdead() then
						return
					end
					-- No target in sight or we failed to kill it
					if not aila.target or not aila.target:isdead() then
						done = true
						break
					end
					targets = aila.findtargets(aila.param.vis, aila.param.prio[i], aila.param.ord)
				end
				if done then
					break
				end
			end
			if not aila.target then
				ai.reactionfire("disable")
				ai.crouch(false)
				aila.search()
			end
		end
	end
end

-- Round end actions
function aila.phase_three ()
	if not ai.actor():isdead() then
		for i = 1, #aila.param.prio do
			local targets = aila.findtargets(aila.param.vis, aila.param.prio[i], aila.param.ord)
			if #targets > 0 and ai.tusforshooting() <= aila.tustouse() then
				aila.target = aila.shoot(targets) or aila.target
				if ai.actor():isdead() then
					return
				end
			end
		end

		local hid
		if ai.hideneeded() then
			hid = aila.hide()
		elseif ai.actor():isinjured() then
			hid = aila.herd() or aila.hide()
		end
		if not hid and ai.actor():morale() == "cower" then
			aila.flee()
		end

		if aila.target then
			aila.target:pos():face()
		end
		ai.reactionfire("enable")
		ai.crouch(true)
	end
end

function aila.prethink ()
	local morale = ai.actor():morale()
	if morale == "panic" then
		if not aila.hide() then
			aila.flee()
		end
		return
	end

	-- copy the default params for this actor class
	local par = aila.params[ai.class()]
	if not par then
		par = aila.params.default
	end
	aila.param = { vis = par.vis, ord = par.ord, pos = par.pos, move = par.move, prio = par.prio }
	-- adjust for morale
	if morale == "rage" or morale == "insane" then
		aila.param.ord = "dist"
		aila.param.pos = "fastest"
		aila.param.move = "rand"
		if morale == "insane" then
			aila.params.prio = {"all"}
		end
	end
end

function aila.think ()
	aila.target = nil
	aila.prethink()
	aila.phase_one()
	aila.phase_two()
	aila.phase_three()
end

--[[
	Team AI example
	No actual team tactics, just run the AI by phases, first short term actions for everybody followed by
	longer term actions for all and close with round end actions for everybody
--]]

aila.phase = 0

function aila.team_think ()
	-- Turn just started set things up.
	if aila.phase < 1 then
		aila.squad = ai.squad()
		aila.actor = 1
		aila.phase = 1
		aila.targets = {}
	-- Run next actor
	else
		aila.actor = aila.actor + 1
	end

	-- We are done with this phase advance to next one.
	if aila.actor > #aila.squad then
		aila.phase = aila.phase + 1
		aila.actor = 1
	end

	-- We are done thinking for the turn.
	if #aila.squad < 1 or aila.phase > 3 then
		aila.phase = 0
		return false
	end

	ai.select(aila.squad[aila.actor])
	ai.print("Phase: ", aila.phase, "Actor: ", aila.squad[aila.actor], aila.actor)
	if not ai.actor():isdead() then
		aila.prethink()
		aila.target = aila.targets[aila.actor]
		if aila.phase == 1 then
			aila.phase_one()
		elseif aila.phase == 2 then
			aila.phase_two()
		else
			aila.phase_three()
		end
		aila.targets[aila.actor] = aila.target
	end

	-- Come back again, we are not done with this turn yet
	return true
end

return aila
