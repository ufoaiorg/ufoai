# Microsoft Developer Studio Project File - Name="qdata" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=qdata - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "qdata.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "qdata.mak" CFG="qdata - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "qdata - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "qdata - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "qdata - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\release_qdata"
# PROP Intermediate_Dir ".\release_qdata"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /G5 /MT /W3 /GX /O2 /I "..\common" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib msvcrt.lib /nologo /subsystem:console /machine:I386 /nodefaultlib /out:"..\..\qdata.exe"

!ELSEIF  "$(CFG)" == "qdata - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\debug_qdata"
# PROP Intermediate_Dir ".\debug_qdata"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /G5 /MTd /W3 /Gm /GX /ZI /Od /I "..\common" /D "WIN32" /D "DEBUG" /D "_CONSOLE" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib msvcrt.lib /nologo /subsystem:console /map /debug /machine:I386 /nodefaultlib

!ENDIF 

# Begin Target

# Name "qdata - Win32 Release"
# Name "qdata - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=..\..\src\tools\ufo2map\common\bspfile.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\ufo2map\common\cmdlib.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\qdata\images.c
# End Source File
# Begin Source File

SOURCE=..\..\src\qcommon\ioapi.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\qdata\common\l3dslib.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\ufo2map\common\lbmlib.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\ufo2map\common\mathlib.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\qdata\models.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\qdata\qdata.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\ufo2map\common\scriplib.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\qdata\sprites.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\qdata\tables.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\qdata\common\trilib.c
# End Source File
# Begin Source File

SOURCE=..\..\src\qcommon\unzip.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\qdata\video.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=..\..\src\tools\ufo2map\common\bspfile.h
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\ufo2map\common\cmdlib.h
# End Source File
# Begin Source File

SOURCE=..\..\src\qcommon\ioapi.h
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\qdata\common\l3dslib.h
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\ufo2map\common\lbmlib.h
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\ufo2map\common\mathlib.h
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\ufo2map\common\qfiles.h
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\ufo2map\common\scriplib.h
# End Source File
# Begin Source File

SOURCE=..\..\src\tools\qdata\common\trilib.h
# End Source File
# Begin Source File

SOURCE=..\..\src\qcommon\unzip.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\src\ports\win32\zlib.lib
# End Source File
# End Group
# End Target
# End Project
