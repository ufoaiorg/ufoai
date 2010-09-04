echo off
echo =====================================================
echo po compiler for UFO:AI (http://sf.net/projects/ufoai)
echo 'msgfmt' utility must be in your path
echo compile all .po files and in src\po and place them
echo into base\i18n\
echo =====================================================

set msgfmtBin=msgfmt.exe
set i18ndir=..\..\base\i18n
set i18nradiantdir=..\..\radiant\i18n
set scriptdir=..\..\contrib\scripts

if NOT EXIST %i18ndir% (
	echo %i18ndir% does not exists;
)

IF "%1"=="" (
echo Missed argument.
echo 	/ufoai or /uforadiant are acceptable
GOTO Continue)


cd ..\..\src\po


:Loop
IF "%1"=="" ( GOTO Continue)
if "%1" == "/ufoai" (
	echo Compiling UFO:AI language files.
	for %%i in (.\ufoai-*.po) DO (
		call :getlocaleforgame %%i
		)
) else if "%1" == "/uforadiant" (
	echo Compiling UFORadiant language files.
	for %%i in (.\uforadiant-*.po) DO (
		call :getlocaleforradiant %%i
		)
) else (
	echo Argument "%1" doesn`t understood.
	echo 	/ufoai or /uforadiant are acceptable
	goto :EOF
	)

SHIFT
goto :Loop

:Continue

cd %scriptdir%

goto :EOF


:getlocaleforgame

set dirname=%~n1
set shortname=%dirname:~6%

	if NOT EXIST "%i18ndir%\%shortname%\LC_MESSAGES" (
		mkdir "%i18ndir%\%shortname%\LC_MESSAGES"
	)
	echo compiling %1 to "%i18ndir%\%shortname%\LC_MESSAGES\ufoai.mo"
	%msgfmtBin% -o "%i18ndir%\%shortname%\LC_MESSAGES\ufoai.mo" %1

exit /b

:getlocaleforradiant

set dirname=%~n1
set shortname=%dirname:~11%

	if NOT EXIST "%i18nradiantdir%\%shortname%\LC_MESSAGES" (
		mkdir "%i18nradiantdir%\%shortname%\LC_MESSAGES"
	)
	echo compiling %1 to "%i18nradiantdir%\%shortname%\LC_MESSAGES\uforadiant.mo"
	%msgfmtBin% -o "%i18nradiantdir%\%shortname%\LC_MESSAGES\uforadiant.mo" %1

exit /b



:EOF
