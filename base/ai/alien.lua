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

		positonshoot (target, position_type, tus) -- Returns a position (pos3 userdata) from which the target actor can be shot or flase in none found.
			target -- *Required* Actor (userdata) to shoot at.
			position_type -- Strategy to find the shooting position:
				"fastest":	(Default) Less pathing cost to get to.
				"nearest":	Closest to the target.
				"farthest":	Farhest from the target.
			tus -- max number of TUs to use for moving + shooting (defaults to use all tus).

		postionhide (team) -- Returns a position (pos3 userdata) for the AI actor to hide in or false if none found.
			team -- Team to hide from, valid values: "phalanx", "civilian" and "alien", defaults to "alien" if AI is civilian and "all but our own team" (which cannot be directly specified) otherwise.

		positionherd (target) -- Returns a position (pos3 userdata) from where target can be used as a meatshield or false if none found.
			target -- *Required* Actor (userdata) to hide behind.

		positionapproach (target, tus, hide) -- Returns a position closer to target or false if none found (Only considers positions along the fastest path to the target)
			target -- *Required* Actor (userdata) to approach to.
			tus	-- Max number of to use to get there (defaults to use all tus).
			hide -- If true the position returned must not be visible by the target actor's team.

		missiontargets (vision, team, sort_order) -- Return a table of positions (userdatas) of visible mission targets.
			vision -- What to see with. Like for see() but only accepts "all" (default), "sight" and "extra"
			team -- Which team mission targets to get. Accepts same values as see() (defaults to "all")
			sort_order -- In which order to sort the targets. LIke for see() but only allows "dist" (default) and "path"

		positionmission (position) -- Returns a position in the area of the given target position, it is meant to have the AI deend mission targets or move to waypoints without all AI actors trying to move into the exact same position.
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

		positionflee () -- Returns a position (userdata) where to flee.

		difficulty () -- Returns the current difficulty of the batlescape from -4 (easiest) to 4 (hardest)

		actor () -- Returns the currently mocving AI actor (userdata)

		class () -- Returns the current AI actor's class (LUA AI subtype).

		hideneeded () -- Returns true if the AI actor need hiding (Low morale or exposed position) false otherwise.


	Actor (userdata) metatable methods (Parameters required unless a default is noted)
		pos (actor) -- Returns the given actor's positon (userdata)

		team (actor) -- Returns the given actor's team.

		shoot (actor, tus) -- Makes the current AI actor try to shoot the given actor, returns true if shots were fired.
			actor -- Actor to shoot at.
			tus -- Max TUs to use for shooting (defaults to using all available TUs)

		throwgrenade (actor, min_group, tus) -- Makes the current AI actor try to throw a grenade at the given actor, returns true if a grenade was thrown.
			actor -- Actor to throw at.
			min_group -- Min number of actors that must be within the grenades splash radius to throw the grenade (defaults to zero)
			tus -- Max TUs to use for shooting (defaults to using all available TUs)

		TU (actor) -- Return the actor's current Time Units.

		HP (actor) -- Return the actor's current Hit Points.

		morale (actor) -- Return the actor's current morale (number)

		isinjured (actor) -- Check if the AI actor is injured (HP < maxHP), returns a boolean value

		isdead (actor) -- Check if the current AI actor is dead, returns a boolean value

		isarmed (actor) -- Check if AI actor has weapons, returns two booleans one for each hand.


	Position (aka pos3 -- userdata) metatable methods (Parameters required unless a default is noted)
		goto (position) -- Makes the current AI actor move to the given position, returns true if the actor reached the target positon.

		face (position) -- Makes the current AI actor try to turn to the direction of the given postion.

		distance (position, type) -- Returns the distance to the given position.
			type -- "dist" (default -- linear distance in map units) or "path" (pathing distance)
--]]

local ai = ai

local params
ai.params = {
	taman =	{ vis = "team", ord = "dist", pos = "fastest", move = "CW", prio = {"~civilian", "civilian"} },
	shevaar = { vis = "extra", ord = "path", pos = "farthest", move = "CCW", prio = {"~civilian", "civilian"} },
	ortnok = { vis = "extra", ord = "HP", pos = "nearest", move = "rand", prio = {"~civilian", "civilian"} },
	bloodspider = { vis = "team", ord = "path", pos = "nearest", move = "hide", prio = {"civilian", "~civilian"} },
	bloodspider_adv = { vis = "team", ord = "path", pos = "nearest", move = "hide", prio = {"~alien"} },
	hovernet = { vis = "team", ord = "dist", pos = "fastest", move = "herd", prio = {"~civilian", "civilian"} },
	hovernet_adv = { vis = "team", ord = "dist", pos = "farthest", move = "herd", prio = {"~civilian", "civilian"} },
	default = { vis = "sight", ord = "dist", pos = "fastest", move = rand, prio = {"~alien"} }
}

function ai.hidetus ()
	return ai.actor():TU() - 4
end

function ai.ismelee()
	local right, left = ai.weapontype()
	return (right == "melee" and (left == "melee" or left == "none")) or (right == "none" and left == "melee")
end

function ai.flee ()
	local flee_pos = ai.positionflee()
	if flee_pos then
		return flee_pos:goto()
	end
	return false
end

function ai.hide ()
	local hide_pos = ai.positionhide()
	if hide_pos then
		return hide_pos:goto()
	end
	return false
end

function ai.herd ()
	local aliens = ai.see("all", "alien", "path")
	if #aliens > 0 then
		for i = 1, #aliens do
			local herd_pos = ai.positionherd(aliens[i])
			if herd_pos then
				return herd_pos:goto()
			end
		end
	end
	return false
end

function ai.approach (targets)
	for i = 1, #targets do
		local near_pos
		for j = 1, 2 do
			if targets[i].pos then
				near_pos = ai.positionapproach(targets[i]:pos(), ai.hidetus(), j == 1)
			else
				near_pos = ai.positionapproach(targets[i], ai.hidetus(), j == 1)
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

function ai.search ()
	-- First check if we have a mission target
	local targets = ai.missiontargets("all", "alien", "path")
	if #targets < 1 then
		-- Check if we can block an enemy target
		for i = 1, #params.prio do
			targets = ai.missiontargets("all", params.prio[i], "path")
			if #targets > 0 then
				break
			end
		end
	end

	local found
	if #targets > 0 then
		for i = 1, #targets do
			local target_pos = ai.positionmission(targets[i])
			if target_pos then
				target_pos:goto()
				found = true
				break;
			end
		end
		-- Can't get to any mission target, try to approach the nearest one
		if not found then
			found = ai.approach(targets)
		end
	end

	-- Nothing found wander around
	if not found then
		if params.move == "herd" then
			ai.herd()
		elseif params.move == "hide" then
			ai.hide()
		else
			local search_tus = (ai.hidetus() - ai.tusforshooting() + 1) / 2
			if search_tus < 1 then
				search_tus =  ai.hidetus() + 1 / 2
			end
			local next_pos = ai.positionwander(params.move, search_tus)
			if next_pos then
				next_pos:goto()
			end
		end
	end
end

function ai.searchweapon ()
	local weapons = ai.findweapons()
	if #weapons > 0 then
		weapons[1]:goto()
		return ai.grabweapon()
	end
	return false
end

function ai.readyweapon ()
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
function ai.shoot (targets)
	local min_group = 3 -- Min enemy group for grenade throw

	for i = 1, #targets do
		local target = targets[i]
		-- Throw a grenade if enough enemies are grouped
		local shot
		-- Be nice in low difficulties and don't throw grenades
		if ai.difficulty() >= 0 then
			 shot = target:throwgrenade(min_group, ai.hidetus())
		end
		-- Shoot
		if not target:isdead() then
			shot = target:shoot(ai.hidetus()) or shot
		end
		if shot then
			return target
		end
	end
	return nil
end

-- Attack the first suitable target in the table - move to position and shoot
function ai.attack (targets)
	for i = 1, #targets do
		-- Get a shoot position
		local shoot_pos = ai.positionshoot(targets[i], params.pos, ai.hidetus())
		if shoot_pos then
			-- Move until target in sight
			shoot_pos:goto()

			local target = ai.shoot{targets[i]}
			if target then
				return target
			end
		end
	end
	return nil
end

-- Engage the first suitable target in the table - approaching if currently out of range
function ai.engage(targets)
	local target
	for i = 1, #targets do
		target = ai.attack{targets[i]}
		if target then
			break
		end
	end
	if not target then
		ai.reactionfire("enable")
		target = ai.approach(targets)
	end
	return target
end

-- Short term reactionary phase
function ai.phase_one ()
	if ai.isfighter() and ai.actor():morale() >= 30 and ai.readyweapon() then
		-- If we don't have enough TUs for shooting try disabling reaction fire to get more available TUs
		if ai.tusforshooting() > ai.actor():TU() then
			ai.reactionfire("disable")
		end
		local targets = ai.see(params.vis, "~civilian")
		while #targets > 0 do
			if ai.actor():HP() < 50 then
				ai.target = ai.shoot(targets)
			else
				ai.target = ai.attack(targets)
			end
			-- We died attacking or cannot attack
			if ai.actor():isdead() or not ai.target or ai.tusforshooting() > ai.actor():TU() then
				return
			end
			targets = ai.see(params.vis, "~civilian")
		end
	end
end

-- Longer term actions
function ai.phase_two ()
	if ai.isfighter() and ai.actor():morale() >= 30 then
		if not ai.readyweapon() then
			if ai.searchweapon() then
				ai.phase_two()
			end
		elseif ai.actor():HP() >= 50 then
			local done
			for i = 1, #params.prio do
				local targets = ai.see(params.vis, params.prio[i], params.ord)
				while #targets > 0 do
					ai.target = ai.engage(targets)
					if not ai.target or not ai.target:isdead() then
						done = true
						break
					end
					targets = ai.see(params.vis, params.prio[i], params.ord)
				end
				if done then
					break
				end
			end
			if not ai.target then
				ai.reactionfire("disable")
				ai.crouch(false)
				ai.search()
			end
		end
	end
end

-- Round end actions
function ai.phase_three ()
	for i = 1, #params.prio do
		local targets = ai.see(params.vis, params.prio[i], params.ord)
		if #targets > 0 and ai.tusforshooting() <= ai.actor():TU() then
			ai.target = ai.shoot(targets) or ai.target
		end
	end

	local hid
	if ai.hideneeded() then
		hid = ai.hide()
	elseif ai.actor():HP() < 50 then
		hid = ai.herd() or ai.hide()
	end
	if not hid and ai.actor():morale() < 30 then
		ai.flee()
	end

	if ai.target then
		ai.target:pos():face()
	end
	ai.reactionfire("enable")
	ai.crouch(true)
end

function think ()
	params = ai.params[ai.class()]
	ai.target = nil
	if not params then
		params = ai.params.default
	end

	ai.phase_one()
	ai.phase_two()
	ai.phase_three()
end
