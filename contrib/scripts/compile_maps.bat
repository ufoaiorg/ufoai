@echo off
setlocal
rem setlocal ensures implicit endlocal call howevert batch exits.
rem so we can goto :EOF  or ctrl-c an at point and env variables
rem and directory will be reset

if EXIST ufo2map.exe (
	goto :Run
)


pushd ..\..

if NOT EXIST ufo2map.exe (
	echo Missing ufo2map
	goto :EOF
)

:Run
rem defaults
set curpath=maps
set searchpath=base/maps
set usecores=%NUMBER_OF_PROCESSORS%
set starttime=%TIME%
set onlynewer=-onlynewer
set param=
set extrasamples=-extra
set shutdownonfinish=false
set quant=

REM loop through args. SHIFT turns %2 into %1 etc

:Loop

IF "%1"=="" ( GOTO Continue)
if EXIST base\maps\%1 (
	set    curpath=maps
	set searchpath=base/maps/%1
) else if "%1"=="/-t" (
	set usecores=1
) else if "%1"=="/help" (
	echo compile_maps ^<options^>
	echo   /-t        always use -t 1 in call to ufo2map, do not use extra processors
	rem parentheses need to be escaped with caret
	echo   /help      print ^(this^) help and exit
	echo   /clean     recompile all maps. default is to only compile
	echo              if .map is newer than.bsp
	echo   /fast      only do one pass on light samples. default is five. Also downscale the lightmap.
	echo   /shutdown  shutdown computer when compilation complete
	echo   path       relative to base\maps.
	echo              eg foo processes *.map in base\maps\foo
	goto :EOF
) else if "%1"=="/clean" (
	set onlynewer=
) else if "%1"=="/check" (
	set param=-check
) else if "%1"=="/fix" (
	set param=-fix
) else if "%1"=="/fast" (
	set extrasamples=
	set quant=-quant 6
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

set ufo2mapparameters=%extrasamples% %param% %onlynewer% -t %usecores% %quant%

for /D %%i in (%searchpath%\*) DO (
		call :compilemap %%i
)
	call :compilemap %searchpath%

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

	if not "%~n1" == "maps" (
		echo ufo2map.exe -v 4 %ufo2mapparameters% %curpath%\%~n1\%%~nxj
		ufo2map.exe -v 4 %ufo2mapparameters% %curpath%\%~n1\%%~nxj || (
			echo.
			echo interrupt or ufo2map returned nonzero, deleting .bsp
			Del %%~dpnj.bsp
			)
		)
	if "%~n1" == "maps" (
		echo ufo2map.exe -v 4 %ufo2mapparameters% %curpath%\%%~nxj
		ufo2map.exe -v 4 %ufo2mapparameters% %curpath%\%%~nxj || (
			echo.
			echo interrupt or ufo2map returned nonzero, deleting .bsp
			Del %%~dpnj.bsp
			)
		)
	)
	echo ...dir "%1" finished
exit /b
