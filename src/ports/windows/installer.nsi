!define PRODUCT_NAME "UFO:Alien Invasion"
!define SHORT_PRODUCT_NAME "UFO:AI"
!define PRODUCT_NAME_DEDICATED "UFO:Alien Invasion Dedicated Server"
!define PRODUCT_VERSION "2.3-dev"
!define PRODUCT_PUBLISHER "UFO:AI Team"
!define PRODUCT_WEB_SITE "http://ufoai.sf.net"
!define PRODUCT_DIR_REGKEY "Software\UFOAI\ufo.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

;SetCompressor bzip2
SetCompressor lzma

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

!define MUI_WELCOMEFINISHPAGE_BITMAP "..\..\..\build\installer.bmp"

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
!insertmacro MUI_LANGUAGE "German"

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
    File /nonfatal "..\..\..\src\docs\tex\*.pdf"
    File "..\..\..\contrib\dlls\*.dll"
    File "..\..\..\*.exe"
    SetOutPath "$INSTDIR\base"
    File "..\..\..\base\*.dll"
    File "..\..\..\base\*.pk3"
    SetOutPath "$INSTDIR\base\i18n"
    SetOutPath "$INSTDIR\base\i18n\cs\LC_MESSAGES"
    File "..\..\..\base\i18n\cs\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\da\LC_MESSAGES"
    File "..\..\..\base\i18n\da\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\de\LC_MESSAGES"
    File "..\..\..\base\i18n\de\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\el\LC_MESSAGES"
    File "..\..\..\base\i18n\el\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\en\LC_MESSAGES"
    File "..\..\..\base\i18n\en\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\es\LC_MESSAGES"
    File "..\..\..\base\i18n\es\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\es_ES\LC_MESSAGES"
    File "..\..\..\base\i18n\es_ES\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\et\LC_MESSAGES"
    File "..\..\..\base\i18n\et\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\fi\LC_MESSAGES"
    File "..\..\..\base\i18n\fi\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\fr\LC_MESSAGES"
    File "..\..\..\base\i18n\fr\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\hu\LC_MESSAGES"
    File "..\..\..\base\i18n\hu\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\it\LC_MESSAGES"
    File "..\..\..\base\i18n\it\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\ja\LC_MESSAGES"
    File "..\..\..\base\i18n\ja\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\pl\LC_MESSAGES"
    File "..\..\..\base\i18n\pl\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\pt\LC_MESSAGES"
    File "..\..\..\base\i18n\pt\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\pt_BR\LC_MESSAGES"
    File "..\..\..\base\i18n\pt_BR\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\ru\LC_MESSAGES"
    File "..\..\..\base\i18n\ru\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\sv\LC_MESSAGES"
    File "..\..\..\base\i18n\sv\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\sl\LC_MESSAGES"
    File "..\..\..\base\i18n\sl\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\base\i18n\th\LC_MESSAGES"
    File "..\..\..\base\i18n\th\LC_MESSAGES\*.mo"

  ;======================================================================
  ; to let the game start up
  ;======================================================================
    SetOutPath "$INSTDIR"

  SectionEnd

  Section "Game Shortcuts" SEC01B
    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}\"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\ufo.exe" "+set vid_fullscreen 1 +set snd_init 1" "$INSTDIR\ufo.exe" 0
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} (safe-mode).lnk" "$INSTDIR\ufo.exe" "+set vid_fullscreen 1 +set vid_mode 6 +set snd_init 0" "$INSTDIR\ufo.exe" 0
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} (safe-mode windowed).lnk" "$INSTDIR\ufo.exe" "+set vid_fullscreen 0 +set vid_mode 6 +set snd_init 0" "$INSTDIR\ufo.exe" 0
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME_DEDICATED}.lnk" "$INSTDIR\ufo_ded.exe" "" "$INSTDIR\ufo.exe_ded" 0
    CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\ufo.exe"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\uninst.exe"
  SectionEnd
SectionGroupEnd

SectionGroup /e "Mapping" SECGROUP02
  Section "Mapping Tools" SEC02
    SetOutPath "$INSTDIR\base\maps"
    File "..\..\..\base\maps\*.map"
    File "..\..\..\base\maps\Makefile.win"
    File "..\..\..\base\maps\compile.p*"
    SetOutPath "$INSTDIR\base\maps\b"
    File "..\..\..\base\maps\b\*.map"
    SetOutPath "$INSTDIR\base\maps\country"
    File "..\..\..\base\maps\country\*.map"
; still in data_source/maps/unfinished
;    SetOutPath "$INSTDIR\base\maps\desert"
;    File "..\..\..\base\maps\desert\*.map"
;    SetOutPath "$INSTDIR\base\maps\oilpipes"
;    File "..\..\..\base\maps\oilpipes\*.map"
;    SetOutPath "$INSTDIR\base\maps\city"
;    File "..\..\..\base\maps\city\*.map"
    SetOutPath "$INSTDIR\base\maps\frozen"
    File "..\..\..\base\maps\frozen\*.map"
    SetOutPath "$INSTDIR\base\maps\ice"
    File "..\..\..\base\maps\ice\*.map"
    SetOutPath "$INSTDIR\base\maps\cemetery"
    File "..\..\..\base\maps\cemetery\*.map"
    SetOutPath "$INSTDIR\base\maps\oriental"
    File "..\..\..\base\maps\oriental\*.map"
    SetOutPath "$INSTDIR\base\maps\tropic"
    File "..\..\..\base\maps\tropic\*.map"
    SetOutPath "$INSTDIR\base\maps\village"
    File "..\..\..\base\maps\village\*.map"
    SetOutPath "$INSTDIR\base\maps\farm"
    File "..\..\..\base\maps\farm\*.map"
    SetOutPath "$INSTDIR\base\maps\shelter"
    File "..\..\..\base\maps\shelter\*.map"
    SetOutPath "$INSTDIR\base\maps\alienb"
    File "..\..\..\base\maps\alienb\*.map"
    SetOutPath "$INSTDIR\base\maps\ufocrash"
    File "..\..\..\base\maps\ufocrash\*.map"
    SetOutPath "$INSTDIR\tools"
    File "..\..\tools\*.ms"
    File "..\..\tools\*.pl"
    ; EULA
    File "..\..\..\contrib\q3radiant\*.doc"
    File "..\..\..\contrib\q3radiant\*.exe"
    File "..\..\..\ufo2map.exe"
    ; RADIANT
    SetOutPath "$INSTDIR\radiant"
    file "..\..\..\radiant\*.dll"
    file "..\..\..\radiant\radiant.exe"
    SetOutPath "$INSTDIR\radiant\bitmaps"
    file "..\..\..\radiant\bitmaps\*.bmp"
    ; RADIANT TRANSLATIONS
    SetOutPath "$INSTDIR\radiant\i18n"
    SetOutPath "$INSTDIR\radiant\i18n\en\LC_MESSAGES"
    File "..\..\..\radiant\i18n\en\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\radiant\i18n\de\LC_MESSAGES"
    File "..\..\..\radiant\i18n\de\LC_MESSAGES\*.mo"
    SetOutPath "$INSTDIR\radiant\i18n\ru\LC_MESSAGES"
    File "..\..\..\radiant\i18n\ru\LC_MESSAGES\*.mo"
    ; RADIANT RUNTIME
    SetOutPath "$INSTDIR\radiant\bin"
    file "..\..\..\radiant\bin\*.exe"
    SetOutPath "$INSTDIR\radiant\etc\gtk-2.0"
    file "..\..\..\radiant\etc\gtk-2.0\gdk-pixbuf.loaders"
    file "..\..\..\radiant\etc\gtk-2.0\gtk.immodules"
    file "..\..\..\radiant\etc\gtk-2.0\gtkrc.default"
    file "..\..\..\radiant\etc\gtk-2.0\gtkrc"
    file "..\..\..\radiant\etc\gtk-2.0\im-multipress.conf"
    SetOutPath "$INSTDIR\radiant\lib\gtk-2.0\2.10.0\engines"
    file "..\..\..\radiant\lib\gtk-2.0\2.10.0\engines\*.dll"
    SetOutPath "$INSTDIR\radiant\lib\gtk-2.0\2.10.0\loaders"
    file "..\..\..\radiant\lib\gtk-2.0\2.10.0\loaders\*.dll"
    SetOutPath "$INSTDIR\radiant\share\themes\MS-Windows\gtk-2.0"
    file "..\..\..\radiant\share\themes\MS-Windows\gtk-2.0\gtkrc"
;   TODO - RADIANT MANUAL
;    SetOutPath "$INSTDIR\radiant\docs"
;    file "..\..\..\radiant\docs\*.html"
;    SetOutPath "$INSTDIR\radiant\docs\css"
;    file /nonfatal "..\..\..\radiant\docs\css\*.css"
;    SetOutPath "$INSTDIR\radiant\docs\images"
;    file /nonfatal "..\..\..\radiant\docs\css\*.jpg"
    SetOutPath "$INSTDIR\radiant\games"
    file "..\..\..\radiant\games\ufoai.game"
    SetOutPath "$INSTDIR\radiant\gl"
    file "..\..\..\radiant\gl\*.glp"
    file "..\..\..\radiant\gl\*.cg"
    SetOutPath "$INSTDIR\radiant\plugins"
    file "..\..\..\radiant\plugins\*.dll"
  SectionEnd

  Section "Mapping Tools Shortcuts" SEC02B
    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}\"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\MAP-Editor.lnk" "$INSTDIR\radiant\radiant.exe" "" "$INSTDIR\radiant\radiant.exe" 0
  SectionEnd
SectionGroupEnd

Section "Source Code" SEC03
  SetOverwrite ifnewer
  SetOutPath "$INSTDIR\build"
  File "..\..\..\build\*.bmp"
  SetOutPath "$INSTDIR\build\projects"
  File "..\..\..\build\projects\*.cbp"
  File "..\..\..\build\projects\*.ico"
  File "..\..\..\build\projects\*.workspace"
  SetOutPath "$INSTDIR\src\client"
  File "..\..\client\*.h"
  File "..\..\client\*.c"
  SetOutPath "$INSTDIR\src\client\menu"
  File "..\..\client\menu\*.h"
  File "..\..\client\menu\*.c"
  SetOutPath "$INSTDIR\src\client\menu\node"
  File "..\..\client\menu\node\*.h"
  File "..\..\client\menu\node\*.c"
  SetOutPath "$INSTDIR\src\client\renderer"
  File "..\..\client\renderer\*.h"
  File "..\..\client\renderer\*.c"
  SetOutPath "$INSTDIR\src\client\campaign"
  File "..\..\client\campaign\*.h"
  File "..\..\client\campaign\*.c"
  SetOutPath "$INSTDIR\src\client\multiplayer"
  File "..\..\client\multiplayer\*.h"
  File "..\..\client\multiplayer\*.c"
  SetOutPath "$INSTDIR\src\docs"
  File "..\..\docs\*.txt"
  SetOutPath "$INSTDIR\src\docs\tex"
  File "..\..\docs\tex\*.tex"
  SetOutPath "$INSTDIR\src\docs\tex\chapters"
  File "..\..\docs\tex\chapters\*.tex"
  SetOutPath "$INSTDIR\src\docs\tex\images"
  File "..\..\docs\tex\images\*.jpg"
  SetOutPath "$INSTDIR\src\game"
  File "..\..\game\*.def"
  File "..\..\game\*.c"
  File "..\..\game\*.h"
  SetOutPath "$INSTDIR\src\game\lua"
  File "..\..\game\lua\*.c"
  File "..\..\game\lua\*.h"
  SetOutPath "$INSTDIR\src\ports"
  SetOutPath "$INSTDIR\src\ports\unix"
  File "..\..\ports\unix\*.h"
  File "..\..\ports\unix\*.c"
  SetOutPath "$INSTDIR\src\ports\windows"
  File "..\..\ports\windows\*.h"
  File "..\..\ports\windows\*.c"
  File "..\..\ports\windows\*.rc"
  SetOutPath "$INSTDIR\src\ports\macosx"
  File "..\..\ports\macosx\*.m"
  SetOutPath "$INSTDIR\src\ports\linux"
  File "..\..\ports\linux\*.c"
  File "..\..\ports\linux\*.xbm"
  File "..\..\ports\linux\*.png"
  SetOutPath "$INSTDIR\src\ports\solaris"
  File "..\..\ports\solaris\*.c"
  SetOutPath "$INSTDIR\src\po"
  File "..\..\po\*.po"
  File "..\..\po\*.pot"
  File "..\..\po\FINDUFOS"
  File "..\..\po\Makefile"
  File "..\..\po\Makevars"
  File "..\..\po\POTFILES.in"
  File "..\..\po\ufopo.pl"
  File "..\..\po\remove-potcdate.sin"
  SetOutPath "$INSTDIR\src\common"
  File "..\..\common\*.c"
  File "..\..\common\*.h"
  SetOutPath "$INSTDIR\src\shared"
  File "..\..\shared\*.c"
  File "..\..\shared\*.h"
  SetOutPath "$INSTDIR\src\server"
  File "..\..\server\*.h"
  File "..\..\server\*.c"

;======================================================================
; tools
;======================================================================
  SetOutPath "$INSTDIR\src\tools"
  File "..\..\tools\*.pl"

; 3dsmax
  File "..\..\tools\*.ms"

; blender
  SetOutPath "$INSTDIR\src\tools\blender"
  File "..\..\tools\blender\*.py"

; radiant
  SetOutPath "$INSTDIR\src\tools\radiant"
  File "..\..\tools\radiant\GPL"
  File "..\..\tools\radiant\LGPL"
  File "..\..\tools\radiant\LICENSE"
  SetOutPath "$INSTDIR\src\tools\radiant\radiant"
  File "..\..\tools\radiant\radiant\*.cpp"
  File "..\..\tools\radiant\radiant\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\radiant\dialogs"
  File "..\..\tools\radiant\radiant\dialogs\*.cpp"
  File "..\..\tools\radiant\radiant\dialogs\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\radiant\sidebar"
  File "..\..\tools\radiant\radiant\sidebar\*.cpp"
  File "..\..\tools\radiant\radiant\sidebar\*.h"

  SetOutPath "$INSTDIR\src\tools\radiant\libs"
  File "..\..\tools\radiant\libs\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\container"
  File "..\..\tools\radiant\libs\container\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\debugging"
  File "..\..\tools\radiant\libs\debugging\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\generic"
  File "..\..\tools\radiant\libs\generic\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\gtkutil"
  File "..\..\tools\radiant\libs\gtkutil\*.h"
  File "..\..\tools\radiant\libs\gtkutil\*.cpp"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\math"
  File "..\..\tools\radiant\libs\math\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\memory"
  File "..\..\tools\radiant\libs\memory\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\modulesystem"
  File "..\..\tools\radiant\libs\modulesystem\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\os"
  File "..\..\tools\radiant\libs\os\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\picomodel"
  File "..\..\tools\radiant\libs\picomodel\*.c"
  File "..\..\tools\radiant\libs\picomodel\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\profile"
  File "..\..\tools\radiant\libs\profile\*.h"
  File "..\..\tools\radiant\libs\profile\*.cpp"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\script"
  File "..\..\tools\radiant\libs\script\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\signal"
  File "..\..\tools\radiant\libs\signal\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\stream"
  File "..\..\tools\radiant\libs\stream\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\string"
  File "..\..\tools\radiant\libs\string\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\libs\xml"
  File "..\..\tools\radiant\libs\xml\*.h"

  SetOutPath "$INSTDIR\src\tools\radiant\plugins"
  File "..\..\tools\radiant\plugins\*.def"
  SetOutPath "$INSTDIR\src\tools\radiant\plugins\archivezip"
  File "..\..\tools\radiant\plugins\archivezip\*.cpp"
  File "..\..\tools\radiant\plugins\archivezip\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\plugins\brushexport"
  File "..\..\tools\radiant\plugins\brushexport\*.cpp"
  File "..\..\tools\radiant\plugins\brushexport\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\plugins\entity"
  File "..\..\tools\radiant\plugins\entity\*.cpp"
  File "..\..\tools\radiant\plugins\entity\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\plugins\image"
  File "..\..\tools\radiant\plugins\image\*.cpp"
  File "..\..\tools\radiant\plugins\image\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\plugins\map"
  File "..\..\tools\radiant\plugins\map\*.cpp"
  File "..\..\tools\radiant\plugins\map\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\plugins\model"
  File "..\..\tools\radiant\plugins\model\*.cpp"
  File "..\..\tools\radiant\plugins\model\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\plugins\shaders"
  File "..\..\tools\radiant\plugins\shaders\*.cpp"
  File "..\..\tools\radiant\plugins\shaders\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\plugins\ufoai"
  File "..\..\tools\radiant\plugins\ufoai\*.cpp"
  File "..\..\tools\radiant\plugins\ufoai\*.h"
  SetOutPath "$INSTDIR\src\tools\radiant\plugins\vfspk3"
  File "..\..\tools\radiant\plugins\vfspk3\*.cpp"
  File "..\..\tools\radiant\plugins\vfspk3\*.h"

  SetOutPath "$INSTDIR\src\tools\radiant\include"
  File "..\..\tools\radiant\include\*.h"

; masterserver
  SetOutPath "$INSTDIR\src\tools\masterserver"
  File "..\..\tools\masterserver\*.php"

; ufo2map
  SetOutPath "$INSTDIR\src\tools\ufo2map"
  File "..\..\tools\ufo2map\*.h"
  File "..\..\tools\ufo2map\*.c"
  SetOutPath "$INSTDIR\src\tools\ufo2map\common"
  File "..\..\tools\ufo2map\common\*.h"
  File "..\..\tools\ufo2map\common\*.c"

  SetOutPath "$INSTDIR\src"
  SetOutPath "$INSTDIR"
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

Function .onVerifyInstDir
  IfFileExists $INSTDIR\*.* Invalid Valid
  Invalid:
  StrCmp $INSTDIR "C:" Break ; Ugly hard-coded constraint, but it should help in most cases.
  StrCmp $INSTDIR "C:\" Break ; "
;  StrCmp $INSTDIR $PROGRAMFILES Break ; Doesn't work.
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

  IntCmp $GAMETEST 1 GameSelected
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
