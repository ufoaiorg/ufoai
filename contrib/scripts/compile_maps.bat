echo off

cd ..\..

if NOT EXIST ufo2map.exe (
	echo Missing ufo2map
	goto end
)

for /D %%i in (base\maps\*) DO (
	echo "...found dir %%i";
	for %%j in (%%i\*.map) DO (
		ufo2map.exe -extra %%j
	)
	echo "...dir %%i finished";
)

for %%i in (base\maps\*.map) DO (
	ufo2map.exe -extra base\maps\%%i
)

:end
