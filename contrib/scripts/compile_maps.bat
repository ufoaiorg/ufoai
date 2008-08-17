@echo off

echo "using %NUMBER_OF_PROCESSORS% processors for radiosity calculations"

set ufo2mapparameters=-extra -onlynewer -t %NUMBER_OF_PROCESSORS%

cd ..\..

if NOT EXIST ufo2map.exe (
	echo Missing ufo2map
	goto :EOF
)

if not "%1"=="" (
	if EXIST base\maps\%1 (
		set curpath=base\maps\%1
		) else (
			echo path "base\maps\%1" not found
			exit
			)
	) else (
		set curpath=base\maps
	)

for /D %%i in (%curpath%\*) DO (
	call :compilemap %%i
)
	call :compilemap %curpath%

goto :EOF


:compilemap
	echo ...found dir "%1"
	for %%j in (%1\*.map) DO (
		ufo2map.exe %ufo2mapparameters% %%j
	)
	echo ...dir "%1" finished
rem CHECK ERRORLEVEL AND REMOVE MAP IF != 0
rem		if errorlevel 1 echo "TODO"
exit /b
