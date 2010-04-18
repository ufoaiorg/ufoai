@echo off

rem lists maps which have not been compiled yet
rem will not work on older versions of windows (the %%~pi type stuff is not available)
rem todo:is it possible to see if the *.map are newer than the *.bsp, and flag this up too
rem then the user could get an idea how long it would take to compile them all using
rem make or the -onlynewer batch.

rem switch to trunk directory
cd ..\..

for /R base\maps %%i in (*.map) DO (
	if NOT EXIST %%~di%%~pi%%~ni.bsp (
		echo todo: %%i
	)
)


:end
rem switch back
cd contrib/scripts
