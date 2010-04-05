
--[[
	AI Entry point.
--]]
function think()

	alien = ai.see("all","alien")

	-- Choose proper action
	if #alien < 1 then

		phalanx = ai.see("all","phalanx")
		if #phalanx < 1 then
			civilian = ai.see("all","civilian")
			if #civilian < 1 then
				ai.crouch()
			else
				friend( civilian[1] )
			end
		else
			friend( phalanx[1] )
		end
	else
		hide()
	end
end


--[[
	Try to get close to the friends.
--]]
function friend( target )

	-- Move until target in sight
	target_pos = target:pos()
	target_pos:goto()

	-- Hide
	hide_pos = ai.positionhide()
	if not hide_pos then -- No position available
		ai.crouch()
		target:face()
	else
		hide_pos:goto()
		ai.crouch()
	end
end


--[[
	Try to hide from aliens
--]]
function hide()

	-- Hide
	hide_pos = ai.positionhide()
	if not hide_pos then -- No position available
		ai.crouch()
	else
		hide_pos:goto()
		ai.crouch()
	end
end
