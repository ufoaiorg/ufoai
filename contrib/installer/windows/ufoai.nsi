!define PRODUCT_NAME "UFO:Alien Invasion"
!define SHORT_PRODUCT_NAME "UFO:AI"
!define PRODUCT_NAME_DEDICATED "UFO:Alien Invasion Dedicated Server"
!ifndef PRODUCT_VERSION
!define PRODUCT_VERSION "2.4-dev"
!endif
!define PRODUCT_PUBLISHER "UFO:AI Team"
!define PRODUCT_WEB_SITE "http://ufoai.sf.net"
!define PRODUCT_DIR_REGKEY "Software\UFOAI\ufo.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

;SetCompressor /SOLID bzip2
SetCompressor /SOLID lzma

; MUI 1.67 compatible ------
!include "MUI.nsh"
!include "LogicLib.nsh"

ShowInstDetails "nevershow"
ShowUninstDetails "nevershow"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "..\..\..\build\projects\ufo.ico"
!define MUI_UNICON "..\..\..\build\projects\ufo.ico"

; Language Selection Dialog Settings
!define MUI_LANGDLL_REGISTRY_ROOT "${PRODUCT_UNINST_ROOT_KEY}"
!define MUI_LANGDLL_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "NSIS:Language"

!define MUI_WELCOMEFINISHPAGE_BITMAP "..\ufoai.bmp"

Var GAMEFLAGS
Var MAPFLAGS
Var GAMETEST
Var MAPTEST
Var GAMEICONFLAGS
Var MAPICONFLAGS

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!define MUI_LICENSEPAGE_CHECKBOX
!insertmacro MUI_PAGE_LICENSE "..\..\..\COPYING"
!define MUI_COMPONENTSPAGE_SMALLDESC
!insertmacro MUI_PAGE_COMPONENTS
; Directory page
!define MUI_DIRECTORYPAGE_VERIFYONLEAVE
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE dirLeave
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
;!define MUI_FINISHPAGE_RUN "$INSTDIR\ufo.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------

Name "${SHORT_PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "ufoai-${PRODUCT_VERSION}-win32.exe"
InstallDir "$PROGRAMFILES\UFOAI-${PRODUCT_VERSION}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Function .onInit
	!insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Function .onInstSuccess
	MessageBox MB_OK "If you want to play multiplayer games, open the TCP port 27910 in your firewall."
FunctionEnd

SectionGroup /e "Game" SECGROUP01
	Section "Game Files" SEC01
		SetOverwrite ifnewer

		SetOutPath "$INSTDIR"
			File "..\..\..\COPYING"
			File "..\..\..\README"
			File /nonfatal "..\..\..\src\docs\tex\*.pdf"
			File "..\..\dlls\*.dll"
			File "..\..\..\ufo.exe"
			File "..\..\..\ufoded.exe"
		SetOutPath "$INSTDIR\base"
			File "..\..\..\base\*.dll"
			File "..\..\..\base\*.pk3"
		SetOutPath "$INSTDIR\base\i18n"
			File /r "..\..\..\base\i18n\*.mo"

	SectionEnd

	Section "Game Shortcuts" SEC01B
		SetOutPath "$INSTDIR"

		CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}\"
		CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\ufo.exe" "+set vid_fullscreen 1 +set snd_init 1" "$INSTDIR\ufo.exe" 0
		CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} (safe-mode).lnk" "$INSTDIR\ufo.exe" "+exec safemode.cfg +set vid_fullscreen 1" "$INSTDIR\ufo.exe" 0
		CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} (safe-mode windowed).lnk" "$INSTDIR\ufo.exe" "+exec safemode.cfg" "$INSTDIR\ufo.exe" 0
		CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME_DEDICATED}.lnk" "$INSTDIR\ufoded.exe" "" "$INSTDIR\ufoded.exe" 0
		CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\ufo.exe"
		CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
		CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\uninst.exe"
	SectionEnd
SectionGroupEnd

SectionGroup /e "Mapping" SECGROUP02
	Section "Mapping Tools" SEC02
		SetOutPath "$INSTDIR\base\maps"
			File "..\..\..\base\maps\compile.p*"
			File /r /x .gitignore "..\..\..\base\maps\*.map"
			File /r /x .gitignore "..\..\..\base\maps\*.ump"
		SetOutPath "$INSTDIR\tools"
			File "..\..\..\src\tools\*.ms"
			File "..\..\..\src\tools\*.pl"
			; EULA
			File "..\..\q3radiant\*.doc"
			File "..\..\q3radiant\*.exe"
		SetOutPath "$INSTDIR"
			File "..\..\..\ufo2map.exe"
		; RADIANT
		SetOutPath "$INSTDIR\radiant"
			File /r /x .gitignore "..\..\..\radiant\*"
			File /r /x .gitignore "..\..\dlls\radiant\*"
	SectionEnd

	Section "Mapping Tools Shortcuts" SEC02B
		SetOutPath "$INSTDIR\radiant"

		CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}\"
		CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\MAP-Editor.lnk" "$INSTDIR\radiant\uforadiant.exe" "" "$INSTDIR\radiant\uforadiant.exe" 0
	SectionEnd
SectionGroupEnd

Section "Source Code" SEC03
	SetOverwrite ifnewer
	SetOutPath "$INSTDIR"
		File /nonfatal "..\..\..\configure"
		File /nonfatal "..\..\..\Makefile"
		File /nonfatal "..\..\..\INSTALL"
		File /nonfatal "..\..\..\COPYING"
		File /nonfatal "..\..\..\LICENSES"
		File /nonfatal "..\..\..\README"
		File /nonfatal "..\..\..\.project"
		File /nonfatal "..\..\..\.cproject"
	SetOutPath "$INSTDIR\build"
		File /r /x .gitignore "..\..\..\build\*.mk"
	SetOutPath "$INSTDIR\build\projects"
		File "..\..\..\build\projects\*.ico"
		File "..\..\..\build\projects\*.xml"
		File "..\..\..\build\projects\*.manifest"
	SetOutPath "$INSTDIR\src"
		File /r /x .gitignore "..\..\..\src\*.h"
		File /r /x .gitignore "..\..\..\src\*.c"
		File /r /x .gitignore "..\..\..\src\*.def"
		File /r /x .gitignore "..\..\..\src\*.rc"
	SetOutPath "$INSTDIR\src\docs"
		File /r /x .gitignore "..\..\..\src\docs\*.*"
	SetOutPath "$INSTDIR\src\ports"
		File /r /x .gitignore "..\..\..\src\ports\*.m"
		File /r /x .gitignore "..\..\..\src\ports\*.xbm"
		File /r /x .gitignore "..\..\..\src\ports\*.png"
	SetOutPath "$INSTDIR\src\po"
		File /r /x .gitignore "..\..\..\src\po\*.*"
	SetOutPath "$INSTDIR\src\tools"
		File /r /x .gitignore "..\..\..\src\tools\*.*"
SectionEnd

Section -AdditionalIcons
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\ufo.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\ufo.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd

; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC01}  "The game and its data. You need this to play."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC01B} "Shortcuts for the game."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC02}  "Mapping (and modelling) tools and map source files."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC02B} "Shortcuts for the mapping tools."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC03}  "C-Source code for UFO:Alien Invasion."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; taken from gaim installer
; GetParent
; input, top of stack  (e.g. C:\Program Files\Poop)
; output, top of stack (replaces, with e.g. C:\Program Files)
; modifies no other variables.
;
; Usage:
;   Push "C:\Program Files\Directory\Whatever"
;   Call GetParent
;   Pop $R0
;   ; at this point $R0 will equal "C:\Program Files\Directory"
;Function GetParent
;   Exch $0 ; old $0 is on top of stack
;   Push $1
;   Push $2
;   StrCpy $1 -1
;   loop:
;     StrCpy $2 $0 1 $1
;     StrCmp $2 "" exit
;     StrCmp $2 "\" exit
;     IntOp $1 $1 - 1
;   Goto loop
;   exit:
;     StrCpy $0 $0 $1
;     Pop $2
;     Pop $1
;     Exch $0 ; put $0 on top of stack, restore $0 to original value
;FunctionEnd

; TODO http://nsis.sourceforge.net/Validating_$INSTDIR_before_uninstall
Function .onVerifyInstDir
  IfFileExists $INSTDIR\*.* Invalid Valid
  Invalid:
  StrCmp $INSTDIR "C:" Break ; Ugly hard-coded constraint, but it should help in most cases.
  StrCmp $INSTDIR "C:\" Break ; "
;  StrCmp $INSTDIR $PROGRAMFILES Break ; Doesn't work.
  ; Push $INSTDIR
  ; for $INSTDIR of e.g. c: or d: GetParent will return ""
  ; Call GetParent
  ; Pop $DIR
  ; StrCmp $DIR "" Break
  Goto Valid
  Break:
  Abort
  Valid:
FunctionEnd

Function dirLeave
  GetInstDirError $0
  ${Switch} $0
    ${Case} 0
      ${Break}
    ${Case} 1
      MessageBox MB_OK "$INSTDIR is not a valid installation path!"
      Abort
      ${Break}
    ${Case} 2
      MessageBox MB_OK "Not enough free space!"
      Abort
      ${Break}
  ${EndSwitch}
  IfFileExists $INSTDIR\*.* Exists NonExists
  Exists:
    MessageBox MB_ICONEXCLAMATION|MB_YESNO|MB_DEFBUTTON2 "The destination folder already exists!$\r$\nAre you sure you want to install into that directory?" IDYES +2
    Abort
  NonExists:
FunctionEnd

Function .onSelChange
  ; This will ensure that you can't install the shortcuts without installing the target files
  SectionGetFlags ${SEC01} $GAMEFLAGS
  SectionGetFlags ${SEC02} $MAPFLAGS
  IntOP $GAMETEST $GAMEFLAGS & ${SF_SELECTED} ; tests the activation bit
  IntOP $MAPTEST $MAPFLAGS & ${SF_SELECTED} ; tests the activation bit

  IntCmp $GAMETEST 1 done
    SectionGetFlags ${SEC01B} $GAMEICONFLAGS
    IntOp $GAMEICONFLAGS $GAMEICONFLAGS & 510 ; Forces to zero the activation bit
    SectionSetFlags ${SEC01B} $GAMEICONFLAGS

  GameSelected:
  IntCmp $MAPTEST 1 done
    SectionGetFlags ${SEC02B} $MAPICONFLAGS
    IntOp $MAPICONFLAGS $MAPICONFLAGS & 510 ; Forces to zero the activation bit
    SectionSetFlags ${SEC02B} $MAPICONFLAGS

  done:
FunctionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) successfully deinstalled."
FunctionEnd

Function un.onInit
!insertmacro MUI_UNGETLANGUAGE
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure that you want to remove $(^Name) and all its data?" IDYES +2
  Abort
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Do you also want to delete your configuration files and saved games?" IDNO +2
  RMDIR /r "$APPDATA\UFOAI"
FunctionEnd

; This uninstaller is unsafe - if a user installs this in the root of a partition, for example, the uninstall will wipe that entire partition.
Section Uninstall
  RMDIR /r $INSTDIR
  RMDIR $INSTDIR
  RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
