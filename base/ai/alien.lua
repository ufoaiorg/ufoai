

--[[
	AI entry point.  Run at the start of the AI's turn.
--]]
function think()

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


--[[
	Try to find a suitable target by wandering around.
--]]
function search ()
end


--[[
	Attempts to approach the target.
]]--
function approach( target )
	ai.print("Can't get to shoot position.")
end


--[[
	Engages target in combat.

	Currently attempts to see target, shoot then hide.
--]]
function engage( target )

	-- Move until target in sight
	shoot_pos = ai.positionshoot(target) -- Get a shoot position
	if not shoot_pos then -- No position available
		approach(target)
	end

	-- Go shoot
	shoot_pos:goto()
	hide_tu = 4 -- Crouch + face

	-- Shoot
	target:shoot(ai.TU() - hide_tu)

	-- Hide
	-- ai.positionhide()
	ai.crouch()
	target:face()
end


