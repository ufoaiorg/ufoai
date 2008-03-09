echo off
echo =====================================================
echo po compiler for UFO:AI (http://sf.net/projects/ufoai)
echo Given path to 'msgfmt' utility as parameter it will
echo compile all .po files and in src/po and place them
echo into base/i18n/
echo =====================================================

cd ../../src/po


if _%1==_ (
goto usage
)

echo ...checking for msgfmt:

if NOT EXIST %1 (
goto missmsgfmt
)
echo Ok.


set i18ndir=../../base/i18n
rem set i18ndir=./i18n

if NOT EXIST %i18ndir% (
echo %i18ndir% does not exists;
rem goto :EOF
)

for %%i in (./*.po) DO (
rem set outDir=%i18ndir%/%%~ni/LC_MESSAGES
if NOT EXIST "%i18ndir%/%%~ni/LC_MESSAGES" (
mkdir "%i18ndir%/%%~ni/LC_MESSAGES"
)
echo compiling %%i to "%i18ndir%/%%~ni/LC_MESSAGES/ufoai.mo"
%1 -o "%i18ndir%/%%~ni/LC_MESSAGES/ufoai.mo" %%i
)
goto :EOF

:missmsgfmt
echo is not executable or does not exist. Exiting...
:usage
echo usage: compile_po.bat E:\path\to\poEdit\bin\msgfmt.exe
