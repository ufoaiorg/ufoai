
function think()
	seen = ai.see()
	phalanx = ai.see("all","phalanx")

	-- Prepare action
	target = phalanx[1]

	-- Move until target in sight
	--ai.moveshoot(target) -- Go to a shoot position
	hide_tu = 4 -- Crouch + face

	-- Shoot
	target:shoot(ai.TU() - hide_tu)

	-- Hide
	--ai.movehide()
	ai.crouch()
	target:face()
end
