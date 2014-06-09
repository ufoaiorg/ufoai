

--[[
	AI entry point.  Run at the start of the AI's turn.
--]]
function think ()
	if ai.HP() < 50 then
		if ai.morale() < 30 then
			hide()
		else
			herd()
		end
	-- Try to make sure we have a weapon
	elseif not readyweapon() then
		herd()
	else
		-- Look around for potential targets.  We prioritize phalanx.
		local phalanx  = ai.see("all","phalanx")
		-- Choose proper action
		if #phalanx < 1 then
			local civilian = ai.see("all","civilian")
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
	ai.pprint("Cant find a target")
end


--[[
	Attempts to approach the target.
--]]
function approach( target )
	local near_pos = ai.positionapproach(target)
	if not near_pos then
		ai.print("Can't approach target")
	else
		near_pos:goto()
	end
end


--[[
	Engages target in combat.

	Currently attempts to see target, shoot then hide.
--]]
function engage( target )
	local hide_tu = 4 -- Crouch + face

	-- Move until target in sight
	local shoot_pos = ai.positionshoot(target, "fastest", ai.TU() - hide_tu) -- Get a shoot position
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
	local hide_pos = ai.positionhide()
	if not hide_pos then -- No position available
	else
		hide_pos:goto()
	end
end


--[[
	Tries to move to herd position
--]]
function herd ()
	local aliens = ai.see("all", "alien")
	if #aliens > 0 then
		local herd_pos = ai.positionherd( aliens[1] )
		if not herd_pos then -- No position available
		else
			herd_pos:goto()
		end
	else
		hide()
	end
end
