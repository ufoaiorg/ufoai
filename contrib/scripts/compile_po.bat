echo off
echo =====================================================
echo po compiler for UFO:AI (http://sf.net/projects/ufoai)
echo 'msgfmt' utility must be in your path
echo compile all .po files and in src\po and place them
echo into base\i18n\
echo =====================================================

set msgfmtBin=msgfmt.exe
set i18ndir=..\..\base\i18n
set scriptdir=..\..\contrib\scripts

if NOT EXIST %i18ndir% (
	echo %i18ndir% does not exists;
)

cd ..\..\src\po
for %%i in (.\*.po) DO (
	if NOT EXIST "%i18ndir%\%%~ni\LC_MESSAGES" (
		mkdir "%i18ndir%\%%~ni\LC_MESSAGES"
	)
	echo compiling %%i to "%i18ndir%\%%~ni\LC_MESSAGES\ufoai.mo"
	%msgfmtBin% -o "%i18ndir%\%%~ni\LC_MESSAGES\ufoai.mo" %%i
)
cd %scriptdir%
