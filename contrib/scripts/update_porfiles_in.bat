echo off
echo =====================================================
echo Updater POTFILES.in from sources.
echo =====================================================

setlocal enableextensions
setlocal enabledelayedexpansion

pushd ..\..\src

set poTMP=po\POTFILES.tmp
set poTMP1=po\POTFILES1.tmp
set poIN=po\POTFILES.in

if exist %poTMP% (del %poTMP%)
if exist %poTMP1% (del %poTMP1%)




findstr /M /S "_(\" ngettext(\"" .\*.c | sort > %poTMP%

echo ./po/OTHER_STRINGS >> %poTMP%

for /f "tokens=*" %%a in ('more^<%poTMP%') do call :fixslashes %%a

move %poTMP1% %poIN%
del %poTMP%

echo Done
popd

exit 0


:fixslashes

set i=%*
set i=%i:\=/%
set i=%i:\r=%
echo %i%>>%poTMP1%

exit /b

:eof