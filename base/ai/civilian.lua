
function think()
	seen = ai.see("all","alien")
	phalanx = ai.see("all","phalanx")

	-- Prepare action
	target = phalanx[1]

	-- Crouch and face phalanx
	if target then
		ai.crouch()
		target:face()
	end
end
