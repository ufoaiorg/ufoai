echo off

set ufo2mapparameters=-extra -onlynewer

cd ..\..

if NOT EXIST ufo2map.exe (
	echo Missing ufo2map
	goto end
)

for /D %%i in (base\maps\*) DO (
	echo "...found dir %%i";
	for %%j in (%%i\*.map) DO (
		ufo2map.exe %ufo2mapparameters% %%j
rem CHECK ERRORLEVEL AND REMOVE MAP IF != 0
rem		if errorlevel 1 echo "TODO"
	)
	echo "...dir %%i finished";
)

for %%i in (base\maps\*.map) DO (
	ufo2map.exe -extra %ufo2mapparameters% %%i
rem CHECK ERRORLEVEL AND REMOVE MAP IF != 0
)

:end
