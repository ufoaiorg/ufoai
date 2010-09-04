@echo off
cd /d "%~dp0"
setlocal
:: setlocal ensures implicit endlocal call howevert batch exits.
:: so we can goto :EOF  or ctrl-c an at point and env variables
:: and directory will be reset



if NOT %OS% == Windows_NT (
	@echo This script will only work on NT systems
	call :error
	exit /b 1
)
if NOT EXIST ..\..\ufo2map.exe (
	@echo Missing ufo2map
	@echo compile UFOAI first
	call :error
	exit /b 1
)


:: defaults
set curpath=maps
set searchpath=base/maps
set usecores=%NUMBER_OF_PROCESSORS%
set starttime=%TIME%
set onlynewer=-onlynewer
set param=
set extrasamples=-extra
set shutdownonfinish=false
set quant=
set md5=false

:: shall we analyze arguments or not
IF "%1"=="" (
	call :choice || exit /b 1
)
IF NOT "%1"=="" (
	@echo work trought ufo2map_args
	IF "%1"=="/?" (
		call :ufo2map_args /help || (
			exit /b 1
		)
	)
	IF NOT "%1"=="/?" (
		call :ufo2map_args %1 %2 %3 %4 %5 %6 %7 %8 %9 || (
			pause
			exit /b 1
		)
	)
)

echo found %NUMBER_OF_PROCESSORS% processors
echo using %usecores% processors
echo compiling maps in %curpath%

set ufo2mapparameters=%extrasamples% %param% %onlynewer% -t %usecores% %quant%


if "%md5%" == "true" (
	if "%onlynewer%" == "-onlynewer" (
		call :md5 /check
	)
)

pushd ..\..
for /D %%i in (%searchpath%\*) DO (
		call :compilemap %%i || exit /b 1
)
call :compilemap %searchpath% || exit /b 1
popd


if "%md5%" == "true" (
	call :md5 /create
)


color 20

@echo.
@echo finished
@echo started at %starttime%
@echo finished at %TIME%
if "%shutdownonfinish%" == "true" (
	reg add "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\RunOnce" /v UFOAI /t REG_SZ /d "%comspec% /k @echo UFOAI make_win32 was successful" /f
	shutdown -s -f -t 5
)
@echo 
endlocal
pause
exit /b 0






:choice
	@echo are you shure to run the build without a parameter?
	@echo use /? for help
	@echo press y to run the script with basic parameters
	set /P coice=
	if /I NOT "%coice%" == "y" (
		exit /b 1
	)
	exit /b 0
goto :EOF


:compilemap
	echo ...found dir "%1"

	for %%j in (%1\*.map) DO (

		if not "%~n1" == "maps" (
			echo ufo2map.exe -v 4 %ufo2mapparameters% %curpath%\%~n1\%%~nxj
			ufo2map.exe -v 4 %ufo2mapparameters% %curpath%\%~n1\%%~nxj || (
				echo.
				echo interrupt or ufo2map returned nonzero, deleting .bsp
				Del "%%~dpnj.bsp"
				call :error
				exit /b 1
			)
		)
		if "%~n1" == "maps" (
			echo ufo2map.exe -v 4 %ufo2mapparameters% %curpath%\%%~nxj
			ufo2map.exe -v 4 %ufo2mapparameters% %curpath%\%%~nxj || (
				echo.
				echo interrupt or ufo2map returned nonzero, deleting .bsp
				Del "%%~dpnj.bsp"
				call :error
				exit /b 1
			)
		)
	)
	echo ...dir "%1" finished
	exit /b 0
goto :EOF








:ufo2map_args
	if EXIST base\maps\%1 (
		set curpath=maps
		set searchpath=base/maps/%1
	) else if "%1"=="/-t" (
		set usecores=1
	) else if "%1"=="/help" (
	rem	echo======max line length is=========================================================
		echo compile_maps ^<options^>
		echo   /-t        always use -t 1 in call to ufo2map, do not use extra processors
		rem parentheses need to be escaped with caret
		echo   /help      print ^(this^) help and exit
		echo   /clean     recompile all maps. default is to only compile
		echo              if .map is newer than.bsp
		echo   /fast      only do one pass on light samples. default is five.
		echo              Also downscale the lightmap.
		echo   /shutdown  shutdown computer when compilation complete
		echo   /md5       Determine map changes based on a hash sum
		echo              also determine ufo2map.exe changes based on the re-version number
		echo              Prevent unnecessary recompile of maps
		echo              but recompile all maps the very first time
		echo   path       relative to base\maps.
		echo              eg foo processes *.map in base\maps\foo
		exit /b 1
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
	) else if "%1"=="/md5" (
		..\..\ufo2map.exe -h | findstr /I /C:"-V --version" || (
			echo ufo2map lack the -V parameter
			echo I'm unable to use /md5
			echo Update your source and recompile ufo2map
			exit /b 1
		)
		for /f %%a in ("md5sum.exe") do if Not Exist "%%~$PATH:a" (
			echo cant find md5sum.exe in your %%path%%
			echo I'm unable to use /md5
			exit /b 1
		)
		set md5=true
	) else (
		echo path "base\maps\%1" not found, %1 argument not understood, try /help
		exit /b 1
	)


	SHIFT
	if NOT "%1" == "" (
		GOTO ufo2map_args
	)
	exit /b 0
goto :EOF







:md5
	@echo Set FSO = CreateObject^("Scripting.FileSystemObject"^) >"%tmp%\ufo2map_md5.vbs"
	@echo Set WshShell = WScript.CreateObject ^("WScript.Shell"^) >>"%tmp%\ufo2map_md5.vbs"
	@echo Set args = WScript.Arguments >>"%tmp%\ufo2map_md5.vbs"
	@echo If args.count ^<^> 1 Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 	WScript.Echo^( Wscript.ScriptName ^& " </create|/check>" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 	WScript.Echo^( Wscript.ScriptName ^& " /create" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 	WScript.Echo^( "   After compiling all maps; this will create map_last_build.md5" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 	WScript.Echo^( Wscript.ScriptName ^& " /check" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 	WScript.Echo^( "   Before compiling all maps; this will check if a map has changed" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 	WScript.Quit^(1^) >>"%tmp%\ufo2map_md5.vbs"
	@echo End if >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo Dim ufoai_source, script_source, map_source, map_path, map_hash, map_hash_string_token, map_last_build, map_last_build_array, map_last_build_write, ufo2map_build >>"%tmp%\ufo2map_md5.vbs"
	@echo ufoai_source = FSO.GetAbsolutePathName^("..\.."^) >>"%tmp%\ufo2map_md5.vbs"
	@echo map_source = ufoai_source ^& "\base\maps" >>"%tmp%\ufo2map_md5.vbs"
	@echo script_source = FSO.GetAbsolutePathName^("."^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 'WScript.Echo^( ufoai_source ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo If args.Item^(0^) = "/check" Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 	If ^(  FSO.FileExists^( script_source ^& "\map_last_build.md5" ^)  ^) Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 		Set map_last_build = FSO.OpenTextFile^( script_source ^& "\map_last_build.md5", 1, 0 ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		map_last_build_array = Split^( map_last_build.ReadAll, vbNewline ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		map_last_build.Close >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo 		Set oExec = WshShell.Exec^( """" ^& ufoai_source ^& "\ufo2map.exe"" -V" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		Do While oExec.Status = 0 >>"%tmp%\ufo2map_md5.vbs"
	@echo 			WScript.Sleep 100 >>"%tmp%\ufo2map_md5.vbs"
	@echo 		Loop >>"%tmp%\ufo2map_md5.vbs"
	@echo 		ufo2map_build = oExec.StdOut.ReadAll^(^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		ufo2map_build = Replace^( ufo2map_build, vbNewline, "", 1, -1, 1 ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		If Not ufo2map_build = map_last_build_array^(0^) Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 			WScript.Echo^( ufo2map_build ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 			WScript.Echo^( "ufo2map build has changed, rebuild all maps" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo '			delete all map files >>"%tmp%\ufo2map_md5.vbs"
	@echo 			Return = WshShell.Run^( "cmd /c del /F /Q /S """ ^& map_source ^& "\*.bsp""", 0, true ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 			WScript.Quit^(0^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		End If >>"%tmp%\ufo2map_md5.vbs"
	@echo 	Else >>"%tmp%\ufo2map_md5.vbs"
	@echo 		WScript.Echo^( "unable to find map_last_build.md5, delete all maps" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo '		delete all map files >>"%tmp%\ufo2map_md5.vbs"
	@echo 		Return = WshShell.Run^( "cmd /c del /F /Q /S """ ^& map_source ^& "\*.bsp""", 0, true ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		WScript.Quit^(0^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 	End If >>"%tmp%\ufo2map_md5.vbs"
	@echo End If >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo strDir = map_source >>"%tmp%\ufo2map_md5.vbs"
	@echo Set objDir = FSO.GetFolder^(strDir^) >>"%tmp%\ufo2map_md5.vbs"
	@echo getInfo^(objDir^) >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo WScript.Quit^(0^) >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo Sub getInfo^(pCurrentDir^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 	For Each aItem In pCurrentDir.Files >>"%tmp%\ufo2map_md5.vbs"
	@echo 		'wscript.Echo aItem.Name >>"%tmp%\ufo2map_md5.vbs"
	@echo 		If LCase^(Right^(Cstr^(aItem.Name^), 3^)^) = "map" Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 			map_path = Replace^( aItem, map_source ^& "\", "", 1, -1, 1 ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo '			WScript.Echo^( map_path ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 			Set oExec = WshShell.Exec^("md5sum -b """ ^& aItem ^& """"^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 			Do While oExec.Status = 0 >>"%tmp%\ufo2map_md5.vbs"
	@echo 				WScript.Sleep 100 >>"%tmp%\ufo2map_md5.vbs"
	@echo 			Loop >>"%tmp%\ufo2map_md5.vbs"
	@echo 			map_hash = oExec.StdOut.ReadAll^(^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 			If InStr^( 1, map_hash, "\", 1 ^) = 1 Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 				map_hash = Right^(  map_hash, Len^( map_hash ^)-1  ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo '				WScript.Echo^( map_hash ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 			End If >>"%tmp%\ufo2map_md5.vbs"
	@echo 			map_hash_string_token = InStr^( 1, map_hash, " ", 1 ^)-1 >>"%tmp%\ufo2map_md5.vbs"
	@echo 			If map_hash_string_token ^> 0 Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 				map_hash = Left^(  map_hash, map_hash_string_token  ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 				WScript.Echo^( map_hash ^& " " ^& map_path ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 				map_last_build_write = map_last_build_write ^& vbNewline ^& map_hash ^& " " ^& map_path >>"%tmp%\ufo2map_md5.vbs"
	@echo 			End If >>"%tmp%\ufo2map_md5.vbs"
	@echo 			If args.Item^(0^) = "/check" Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 				For counter = 1 To UBound^( map_last_build_array ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 					map_last_build_array_string_split = InStr^( 1, map_last_build_array^(counter^), " ", 1 ^)-1 >>"%tmp%\ufo2map_md5.vbs"
	@echo 					map_last_build_array_string_hash = Left^( map_last_build_array^(counter^), map_last_build_array_string_split ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 					map_last_build_array_string_path = Right^(  map_last_build_array^(counter^), Len^(  map_last_build_array^(counter^)  ^)-map_last_build_array_string_split-1  ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo '					WScript.Echo^( map_last_build_array^(counter^) ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo '					WScript.Echo^( map_last_build_array_string_hash ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo '					WScript.Echo^( map_last_build_array_string_path ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 					If map_hash = map_last_build_array_string_hash And map_path = map_last_build_array_string_path Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 						WScript.Echo^( "unchanged" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 						Exit For >>"%tmp%\ufo2map_md5.vbs"
	@echo 					End If >>"%tmp%\ufo2map_md5.vbs"
	@echo 					If counter = UBound^( map_last_build_array ^) Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 						WScript.Echo^( "changed" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 						WScript.Echo^(  "delte: " ^& map_source ^& "\" ^& Left^(  map_path, Len^( map_path^)-3  ^) ^& "bsp"  ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo '						delete bsp >>"%tmp%\ufo2map_md5.vbs"
	@echo 						If FSO.FileExists^( map_source ^& "\" ^& Left^(  map_path, Len^( map_path^)-3  ^) ^& "bsp" ^) Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 							Set file_delete = FSO.GetFile^( map_source ^& "\" ^& Left^(  map_path, Len^( map_path^)-3  ^) ^& "bsp" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 							file_delete.Delete >>"%tmp%\ufo2map_md5.vbs"
	@echo 						End If >>"%tmp%\ufo2map_md5.vbs"
	@echo 					End If >>"%tmp%\ufo2map_md5.vbs"
	@echo 				Next >>"%tmp%\ufo2map_md5.vbs"
	@echo 			End If >>"%tmp%\ufo2map_md5.vbs"
	@echo 		End If >>"%tmp%\ufo2map_md5.vbs"
	@echo 	Next >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo 	For Each aItem In pCurrentDir.SubFolders >>"%tmp%\ufo2map_md5.vbs"
	@echo 		'wscript.Echo aItem.Name ^& " passing recursively" >>"%tmp%\ufo2map_md5.vbs"
	@echo 		getInfo^(aItem^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 	Next >>"%tmp%\ufo2map_md5.vbs"
	@echo 	If args.Item^(0^) = "/create" Then >>"%tmp%\ufo2map_md5.vbs"
	@echo 		Set oExec = WshShell.Exec^( """" ^& ufoai_source ^& "\ufo2map.exe"" -V" ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		Do While oExec.Status = 0 >>"%tmp%\ufo2map_md5.vbs"
	@echo 			WScript.Sleep 100 >>"%tmp%\ufo2map_md5.vbs"
	@echo 		Loop >>"%tmp%\ufo2map_md5.vbs"
	@echo 		ufo2map_build = oExec.StdOut.ReadAll^(^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		ufo2map_build = Replace^( ufo2map_build, vbNewline, "", 1, -1, 1 ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo. >>"%tmp%\ufo2map_md5.vbs"
	@echo 		Set map_last_build = FSO.CreateTextFile^( script_source ^& "\map_last_build.md5", true, false ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		map_last_build.Write^( ufo2map_build ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		map_last_build.Write^( map_last_build_write ^) >>"%tmp%\ufo2map_md5.vbs"
	@echo 		map_last_build.Close >>"%tmp%\ufo2map_md5.vbs"
	@echo 	End If >>"%tmp%\ufo2map_md5.vbs"
	@echo End Sub >>"%tmp%\ufo2map_md5.vbs"

	cscript "%tmp%\ufo2map_md5.vbs" %1
	del /F /Q "%tmp%\ufo2map_md5.vbs"
	exit /b 0
goto :EOF





:error
	color 40

	@echo started at %starttime%
	@echo finished at %TIME%
	if "%shutdownonfinish%" == "true" (
		reg add "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\RunOnce" /v UFOAI /t REG_SZ /d "%comspec% /k @echo UFOAI make_win32 was NOT successful" /f
		shutdown /s /f /t 0
	)
	@echo 
	endlocal
	pause
goto :EOF
