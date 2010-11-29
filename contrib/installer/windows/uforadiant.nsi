!define PRODUCT_NAME "UFORadiant"
!define SHORT_PRODUCT_NAME "UFORadiant"
!define PRODUCT_NAME_DEDICATED "UFO:Alien Invasion Mapeditor"
!ifndef PRODUCT_VERSION
!define PRODUCT_VERSION "1.6.0"
!endif
!define PRODUCT_PUBLISHER "UFO:AI Team"
!define PRODUCT_WEB_SITE "http://ufoai.sf.net"
!define PRODUCT_DIR_REGKEY "Software\UFORadiant\uforadiant.exe"

;SetCompressor /SOLID bzip2
SetCompressor /SOLID lzma

; MUI 1.67 compatible ------
!include "MUI.nsh"

ShowInstDetails "nevershow"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "..\..\..\build\projects\radiant.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "uforadiant.bmp"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

; MUI end ------

Name "${SHORT_PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "uforadiant-${PRODUCT_VERSION}-win32.exe"
InstallDir "$PROGRAMFILES\UFORadiant-${PRODUCT_VERSION}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show

Function .onInit
	!insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd


Section "UFORadiant"
	SetOverwrite ifnewer
	SetOutPath "$INSTDIR"
		File /r /x .gitignore "..\..\..\radiant\*"
		File /r /x .gitignore "..\..\dlls\radiant\*"

		SetOutPath "$INSTDIR"
		CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}\"
		CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\MAP-Editor.lnk" "$INSTDIR\uforadiant.exe" "" "$INSTDIR\uforadiant.exe" 0
		WriteIniStr "$INSTDIR\Mapping Tutorials.url" "InternetShortcut" "URL" "http://ufoai.ninex.info/wiki/index.php/Mapping"
SectionEnd


Section Uninstall
	RMDIR /r $INSTDIR
SectionEnd
