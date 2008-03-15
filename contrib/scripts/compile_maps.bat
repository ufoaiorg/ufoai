echo off

cd ../..

if NOT EXIST ufo2map.exe (
	echo Missing ufo2map
	goto end
)

for %%i in (base/maps/*.map) DO (
	ufo2map.exe -extra base/maps/%%i
)

:end
