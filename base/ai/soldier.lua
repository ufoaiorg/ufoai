--[[
	AI Entry point.
--]]
function think()
	local alien = ai.see("all", "alien")[1]
	local civ = ai.see("all", "civilian")[1]
	local phalanx = ai.see("all", "phalanx")[1]

	if alien then
		local alienDist = ai.distance(alien)
		-- check danger close
		if alienDist < 128 then
			if phalanx then
				local phalanxDist = ai.distance(phalanx)

				-- check alien closer than phalanx
				if alienDist < phalanxDist then
					engage( alien[1] )
				else
					if ai.isinjured() then
						hide(alien:team())
					else
						herd(phalanx)
					end
				end
			else
				engage( alien[1] )
			end
		else
			if phalanx then
				local phalanxDist = ai.distance(phalanx)
				-- check alien closer than phalanx
				if alienDist < phalanxDist then
					hide(alien:team())
				else
					if civ then
						local civDist = ai.distance(civ)

						-- check phalanx closer than civilian
						if phalanxDist < civDist then
							herd(phalanx)
						else
							herd(civ)
						end
					else
						herd(phalanx)
					end
				end
			else
				if civ then
					local civDist = ai.distance(civ)

					-- check alien closer than civilian
					if alienDist < civDist then
						engage( alien[1] ) 
					else
						herd(civ)
					end
				else
					hide(alien:team())
				end
			end
		end
	else
		if civ then
			herd(civ)
		else
			if phalanx then
				herd(phalanx) 
			else
				engage( alien[1] )
			end
		end
	end
end

--[[
	Tries to move to herd position
--]]
function herd(target)
	local pos = ai.positionherd(target)
	if pos then
		pos:goto()
	end
end

--[[
	Tries to move to hide position
--]]
function hide(team)
	local pos = ai.positionhide(team)
	if pos then
		pos:goto()
	end
	ai.crouch(false)
end
