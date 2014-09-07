--[[
	AI moduel methods, these work on the currently moving AI actor (Unless noted otherwise parameters are optional):
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

		isinjured () -- Check if the AI actor is injured (HP < maxHP), returns a boolean value

		isdead () -- Check if the current AI actor is dead, returns a boolean value

		reactionfire (state) -- Check if Reaction Fire is enabled (returns a boolean value) and optionally change it
			state -- Ask to change the RF state:
				"disable":	Disable reaction fire.
				Any other string: Enable reaction fire.

		isfighter () -- Chack if the current AI actor is capable of fighting (can use weapons or has an onlyweapon)

		isarmed () -- Check if AI actor has weapons, returns two booleans one for each hand.

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
			team -- Team to hide from, vlaid values: "phalanx", "civilian" and "alien", defaults to "alien" if AI is civilian and "all but our own team" (not a valid value) otherwise.

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

		distance (target) -- Returns the distance to the target.
			target -- *Required* Actor (userdata) to get the distance to.

		difficulty () -- Returns the current difficulty of the batlescape from -4 (easiest) to 4 (hardest)


	Actor (userdata) metatable methods (Parameters required unless a default is noted)
		pos (actor) -- Returns the given actor's positon (userdata)

		team (actor) -- Returns the given actor's team.

		face (actor) -- Makes the current AI actor try .to turn to the direction of the given actor

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

	Position (aka pos3 -- userdata) metatable methods (Parameters required unless a default is noted)
		goto (position) -- Makes the current AI actor move to the given position, returns true if the actor reached the target positon.

		approach (position) -- Makes the current AI actor move as far towards the given position as possible (Only considers positions along the fastest path to the target)

		face (position) -- Makes the current AI actor try to turn to the direction of the given postion.

		distance (position) -- Returns the distance to the given position.
--]]

--[[
	AI entry point.  Run at the start of the AI's turn.
--]]
function think ()
	if ai.actor:HP() < 50 then
		if ai.actor:morale() < 30 then
			hide()
		else
			herd()
		end
	-- Try to make sure we have a weapon
	elseif not readyweapon() then
		herd()
	else
		-- Look around for potential targets.  We prioritize phalanx.
		local phalanx  = ai.see("team", "phalanx")
		-- Choose proper action
		if #phalanx < 1 then
			local civilian = ai.see("team", "civilian")
			if #civilian < 1 then
				search()
			else
				engage( civilian )
			end
		else
			engage( phalanx )
		end
	end
end


--[[
	Search for a new weapon to grab
--]]
function searchweapon ()
	local weapons = ai.findweapons()
	if #weapons > 0 then
		weapons[1]:goto()
		return ai.getweapon()
	end
	return false
end


--[[
	Try to make sure to have a working weapon
--]]
function readyweapon ()
	local has_right, has_left = ai.isarmed()
	local right_ammo, left_ammo = ai.roundsleft()
	if not right_ammo and not left_ammo then
		if has_right then
			ai.reload("right")
		elseif has_left then
			ai.reload("left")
		end
	end

	has_right, has_left = ai.isarmed()
	if not has_right and not has_left and not ai.getweapon() then
		return searchweapon()
	end

	return true
end


--[[
	Try to find a suitable target by wandering around.
--]]
function search ()
	-- First check if we have a mission target
	local targets = ai.missiontargets("all", "alien")
	if #targets < 1 then
		-- Check if we can block an enemy target
		targets = ai.missiontargets("all", "phalanx")
	end

	local found = false
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
			targets[0]:approach()
			found = true
		end
	end

	if not found then
		local next_pos = ai.positionwander()
		if next_pos then
			next_pos:goto()
		end
	end
end


--[[
	Attempts to approach the target.
--]]
function approach( targets )
	for i = 1, #targets do
		local near_pos = ai.positionapproach( targets[i] )
		if near_pos then
			near_pos:goto()
			return targets[i]
		end
	end
	return nil
end


--[[
	Engages target in combat.

	Currently attempts to see target, shoot then hide.
--]]
function engage( targets )
	local target = nil
	local hide_tu = 4 -- Crouch + face
	local min_group = 3 -- Min enemy group for grenade throw

	local done = false
	for i = 1, #targets do
		target = targets[i]
		-- Move until target in sight
		local shoot_pos = ai.positionshoot(target, "fastest", ai.actor:TU() - hide_tu) -- Get a shoot position
		if shoot_pos then
			-- Go shoot
			shoot_pos:goto()

			-- Throw a grenade if enough enemies are grouped
			target:throwgrenade(min_group, ai.actor:TU() - hide_tu)
			-- Shoot
			target:shoot(ai.actor:TU() - hide_tu)
			done = i
			break
		end
	end

	if not done then
		target = approach(targets)
	end

	-- Hide
	hide()
	if target then
		target:face()
	else
		search()
	end
	ai.crouch(true)
end


--[[
	Tries to move to hide position
--]]
function hide ()
	local hide_pos = ai.positionhide()
	if hide_pos then
		hide_pos:goto()
	end
end


--[[
	Tries to move to herd position
--]]
function herd ()
	local aliens = ai.see("all", "alien")
	if #aliens > 0 then
		for i = 1, #aliens do
			local herd_pos = ai.positionherd(aliens[i])
			if herd_pos then
				herd_pos:goto()
				return
			end
		end
	else
		hide()
	end
end
