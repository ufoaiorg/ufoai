
--[[
	AI Entry point.
--]]
function think()
	phalanx = ai.see("all","phalanx")

	-- Prepare action
	target = phalanx[1]

	-- Crouch and face phalanx
	if target then
		ai.crouch()
		target:face()
	end
end
