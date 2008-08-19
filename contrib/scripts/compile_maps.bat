@echo off
setlocal
rem setlocal ensures implicit endlocal call howevert batch exits.
rem so we can goto :EOF  or ctrl-c an at point and env variables
rem and directory will be reset

pushd ..\..

if NOT EXIST ufo2map.exe (
	echo Missing ufo2map
	goto :EOF
)

rem defaults
set curpath=base\maps
set usecores=%NUMBER_OF_PROCESSORS%
set starttime=%TIME%
set onlynewer=-onlynewer
set extrasamples=-extra
set shutdownonfinish=false

REM loop through args. SHIFT turns %2 into %1 etc

:Loop

IF "%1"=="" ( GOTO Continue)
if EXIST base\maps\%1 (
	set curpath=base\maps\%1
) else if "%1"=="/-t" (
	set usecores=1
) else if "%1"=="/help" (
	echo compile_maps ^<options^>
	echo   /-t        always use -t 1 in call to ufo2map, do not use extra processors
	rem parentheses need to be escaped with caret
	echo   /help      print ^(this^) help and exit
	echo   /clean     recompile all maps. default is to only compile
	echo              if .map is newer than.bsp
	echo   /fast      only do one pass on light samples. default is five.
	echo   /shutdown  shutdown computer when compilation complete
	echo   path       relative to base\maps.
	echo              eg foo processes *.map in base\maps\foo
	goto :EOF
) else if "%1"=="/clean" (
	set onlynewer=
) else if "%1"=="/fast" (
	set extrasamples=
) else if "%1"=="/shutdown" (
	set shutdownonfinish=true
) else (
	echo path "base\maps\%1" not found, %1 argument not understood, try /help
	goto :EOF
)


SHIFT
GOTO Loop
:Continue

echo found %NUMBER_OF_PROCESSORS% processors
echo using %usecores% processors
echo compiling maps in %curpath%

set ufo2mapparameters=%extrasamples% %onlynewer% -t %usecores%

for /D %%i in (%curpath%\*) DO (
	call :compilemap %%i
)
	call :compilemap %curpath%

echo.
echo  started at %starttime%
echo finished at %TIME%

if "%shutdownonfinish%"=="true" (
	shutdown /s
)

goto :EOF

:compilemap
	echo ...found dir "%1"
	for %%j in (%1\*.map) DO (
		echo ufo2map.exe %ufo2mapparameters% %%j
		ufo2map.exe %ufo2mapparameters% %%j || (
			echo.
			echo interrupt or ufo2map returned nonzero, deleting .bsp
			Del %%~dpnj.bsp
		)
	)
	echo ...dir "%1" finished
exit /b
