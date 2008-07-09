
function think()
	seen = ai.see()
	phalanx = ai.see("all","phalanx")

	phalanx[1]:shoot()
end
