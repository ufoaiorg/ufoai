@echo off
setlocal

pushd ..\..

if NOT EXIST ufo2map.exe (
	echo Missing ufo2map
	goto :EOF
)

rem defaults
set curpath=base\maps
set usecores=%NUMBER_OF_PROCESSORS%
set starttime=%TIME%

REM loop through args. SHIFT turns %2 into %1 etc
:Loop
IF "%1"=="" GOTO Continue
if EXIST base\maps\%1 (
	set curpath=base\maps\%1
) else (
	if "%1"=="/-t" (
		set usecores=1
	) else (
		if "%1"=="/help" (
			echo compile_maps [/-t] [/help] [path]
			echo   /-t    always use -t 1 in call to ufo2map, do not use extra processors
			rem parentheses need to be escaped with caret
			echo   /help  print ^(this^) help and exit
			echo   path   relative to base\maps. eg foo processes *.map in base\maps\foo
			GOTO End
		) else (
			echo path "base\maps\%1" not found, %1 argument not understood, try /help
			GOTO End
		)
	)
)
SHIFT
GOTO Loop
:Continue

echo found %NUMBER_OF_PROCESSORS% processors
echo using %usecores% processors
echo compiling maps in %curpath%

set ufo2mapparameters=-extra -onlynewer -t %usecores%

for /D %%i in (%curpath%\*) DO (
	call :compilemap %%i
)
	call :compilemap %curpath%

echo.
echo  started at %starttime%
echo finished at %TIME%

popd
	
goto :EOF

:compilemap
	echo ...found dir "%1"
	for %%j in (%1\*.map) DO (
		ufo2map.exe %ufo2mapparameters% %%j
	)
	echo ...dir "%1" finished
rem CHECK ERRORLEVEL AND REMOVE MAP IF != 0
rem		if errorlevel 1 echo "TODO"
GOTO End

:End
cd contrib\scripts
exit /b