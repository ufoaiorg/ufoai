

--[[
	AI entry point.  Run at the start of the AI's turn.
--]]
function think ()

	-- Try to make sure we have a weapon
	if not readyweapon() then
		-- No weapon, retreat
		teammates = ai.see("all", "alien")
		if #teammates > 0 then
			herd( teammates[1] )
		else
			hide()
		end
	else
		-- Look around for potential targets.  We prioritize phalanx.
		phalanx  = ai.see("all","phalanx")
		-- Choose proper action
		if #phalanx < 1 then
			civilian = ai.see("all","civilian")
			if #civilian < 1 then
				search()
			else
				engage( civilian[1] )
			end
		else
			engage( phalanx[1] )
		end
	end
end


--[[
	Search for a new weapon to grab
--]]
function searchweapon ()
	-- TODO: actually implement the search
	ai.print("No weapon available")

	return false
end


--[[
	Try to make sure to have a working weapon
--]]
function readyweapon ()
	has_right, has_left = ai.isarmed()
	right_ammo, left_ammo = ai.roundsleft()
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
	ai.pprint("Cant find a target")
end


--[[
	Attempts to approach the target.
--]]
function approach( target )
	near_position = ai.positionapproach(target)
	if not near_position then
		ai.print("Can't approach target")
	else
		near_position:goto()
	end
end


--[[
	Engages target in combat.

	Currently attempts to see target, shoot then hide.
--]]
function engage( target )
	hide_tu = 4 -- Crouch + face

	-- Move until target in sight
	shoot_pos = ai.positionshoot(target, "fastest", ai.TU() - hide_tu) -- Get a shoot position
	if not shoot_pos then -- No position available
		approach(target)
	else
		-- Go shoot
		shoot_pos:goto()

	-- Shoot
	target:shoot(ai.TU() - hide_tu)
	end


	-- Hide
	hide()
	ai.crouch(true)
	target:face()
end


--[[
	Tries to move to hide position
--]]
function hide ()
	hide_pos = ai.positionhide()
	if not hide_pos then -- No position available
	else
		hide_pos:goto()
	end
end


--[[
	Tries to move to herd position
--]]
function herd (target)
	herd_pos = ai.positionherd(target)
	if not herd_pos then -- No position available
	else
		herd_pos:goto()
	end
end
