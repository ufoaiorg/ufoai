local ails = { }

--[[
	AI Entry point.
--]]
function ails.think()
	local alien = ai.see("all", "alien")[1]
	local civ = ai.see("all", "civilian")[1]
	local phalanx = ai.see("all", "phalanx")[1]

	if alien then
		local alienDist = alien:pos():distance()
		-- check danger close
		if alienDist < 128 then
			if phalanx then
				local phalanxDist = phalanx:pos():distance()

				-- check alien closer than phalanx
				if alienDist < phalanxDist then
					ails.engage( alien[1] )
				else
					if ai.isinjured() then
						ails.hide(alien:team())
					else
						ails.herd(phalanx)
					end
				end
			else
				ails.engage( alien[1] )
			end
		else
			if phalanx then
				local phalanxDist = phalanx:pos():distance()
				-- check alien closer than phalanx
				if alienDist < phalanxDist then
					ails.hide(alien:team())
				else
					if civ then
						local civDist = civ:pos():distance()

						-- check phalanx closer than civilian
						if phalanxDist < civDist then
							ails.herd(phalanx)
						else
							ails.herd(civ)
						end
					else
						ails.herd(phalanx)
					end
				end
			else
				if civ then
					local civDist = civ:pos():distance()

					-- check alien closer than civilian
					if alienDist < civDist then
						ails.engage( alien[1] )
					else
						ails.herd(civ)
					end
				else
					ails.hide(alien:team())
				end
			end
		end
	else
		if civ then
			ails.herd(civ)
		else
			if phalanx then
				ails.herd(phalanx)
			else
				ails.engage( alien[1] )
			end
		end
	end
end

--[[
	Tries to move to herd position
--]]
function ails.herd(target)
	local pos = ai.positionherd(target)
	if pos then
		pos:goto()
	end
end

--[[
	Tries to move to hide position
--]]
function ails.hide(team)
	local pos = ai.positionhide(team)
	if pos then
		pos:goto()
	end
	ai.crouch(true)
end

function ails.engage (target)
-- Todo
end

return ails