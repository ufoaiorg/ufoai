
function think()
	seen = ai.see()
	phalanx = ai.see("all","phalanx")

	-- Prepare action
	target = phalanx[1]

	-- Move until target in sight
	shoot_pos = ai.positionshoot(target) -- Go to a shoot position
	if shoot_pos then -- We can actually shoot target

		shoot_pos:goto()
		hide_tu = 4 -- Crouch + face

		-- Shoot
		target:shoot(ai.TU() - hide_tu)

		-- Hide
		-- ai.positionhide()
		ai.crouch()
		target:face()
	else
		ai.print("Can't get to shoot position.")
	end
end
