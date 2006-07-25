# Microsoft Developer Studio Project File - Name="ufo" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101
# TARGTYPE "Win32 (ALPHA) Application" 0x0601

CFG=ufo - Win32 Debug Alpha
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ufo.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ufo.mak" CFG="ufo - Win32 Debug Alpha"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ufo - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "ufo - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "ufo - Win32 Debug Alpha" (based on "Win32 (ALPHA) Application")
!MESSAGE "ufo - Win32 Release Alpha" (based on "Win32 (ALPHA) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "ufo - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\"
# PROP Intermediate_Dir ".\release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /G5 /MT /W4 /GX /Zd /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib /nologo /subsystem:windows /machine:I386
# SUBTRACT LINK32 /incremental:yes /debug /nodefaultlib

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\debug"
# PROP Intermediate_Dir ".\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /G5 /MTd /W3 /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /YX /FD /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib /nologo /subsystem:windows /incremental:no /map /debug /machine:I386 /out:"..\ufo.exe"
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ufo__"
# PROP BASE Intermediate_Dir "ufo__"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\DebugAxp"
# PROP Intermediate_Dir ".\DebugAxp"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /Gt0 /W3 /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /YX /FD /c
# SUBTRACT BASE CPP /Gy
# ADD CPP /nologo /QA21164 /MTd /Gt0 /W3 /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "C_ONLY" /YX /FD /QAieee1 /c
# SUBTRACT CPP /Fr
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /debug /machine:ALPHA
# SUBTRACT BASE LINK32 /nodefaultlib
# ADD LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib /nologo /subsystem:windows /debug /machine:ALPHA
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ufo__"
# PROP BASE Intermediate_Dir "ufo__"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\ReleaseAXP"
# PROP Intermediate_Dir ".\ReleaseAXP"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /Gt0 /W3 /GX /Zd /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /QA21164 /MT /Gt0 /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "C_ONLY" /YX /FD /QAieee1 /c
# SUBTRACT CPP /Z<none> /Fr
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /machine:ALPHA
# SUBTRACT BASE LINK32 /debug /nodefaultlib
# ADD LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib /nologo /subsystem:windows /machine:ALPHA
# SUBTRACT LINK32 /debug /nodefaultlib

!ENDIF 

# Begin Target

# Name "ufo - Win32 Release"
# Name "ufo - Win32 Debug"
# Name "ufo - Win32 Debug Alpha"
# Name "ufo - Win32 Release Alpha"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\win32\cd_win.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CD_WI=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CD_WI=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_actor.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_basemanagement.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_campaign.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_cin.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CL_CI=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CL_CI=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_fx.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CL_FX=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CL_FX=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_input.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CL_IN=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CL_IN=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_le.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_main.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CL_MA=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CL_MA=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_menu.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_parse.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CL_PA=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CL_PA=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_particle.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_research.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_scrn.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CL_SC=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CL_SC=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_sequence.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_team.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_ufopedia.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\cl_view.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CL_VI=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CL_VI=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\cmd.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CMD_C=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CMD_C=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\cmodel.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CMODE=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CMODE=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\common.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_COMMO=\
	".\client\anorms.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_COMMO=\
	".\client\anorms.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\win32\conproc.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CONPR=\
	".\win32\conproc.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CONPR=\
	".\win32\conproc.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\console.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CONSO=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CONSO=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\crc.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CRC_C=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CRC_C=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\cvar.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_CVAR_=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_CVAR_=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\files.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_FILES=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_FILES=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\win32\in_win.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_IN_WI=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\win32\winquake.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_IN_WI=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\win32\winquake.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\keys.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_KEYS_=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_KEYS_=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\md4.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\net_chan.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_NET_C=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_NET_C=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\win32\net_wins.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_NET_W=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_NET_W=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\game\q_shared.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_Q_SHA=\
	".\game\q_shared.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_Q_SHA=\
	".\game\q_shared.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\win32\q_shwin.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_Q_SHW=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\win32\winquake.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_Q_SHW=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\win32\winquake.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\qcommon\scripts.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\snd_ref.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SND_D=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\snd_loc.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SND_D=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\snd_loc.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\snd_mem.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SND_M=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\snd_loc.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SND_M=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\snd_loc.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\snd_mix.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SND_MI=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\snd_loc.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SND_MI=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\snd_loc.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\win32\snd_win.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SND_W=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\snd_loc.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\win32\winquake.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SND_W=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\snd_loc.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\win32\winquake.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\server\sv_ccmds.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SV_CC=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SV_CC=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\server\sv_game.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SV_GA=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SV_GA=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\server\sv_init.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SV_IN=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SV_IN=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\server\sv_main.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SV_MA=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SV_MA=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\server\sv_send.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SV_SE=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SV_SE=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\server\sv_user.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SV_US=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SV_US=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\server\sv_world.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SV_WO=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SV_WO=\
	".\game\game.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\server\server.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\win32\sys_win.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_SYS_W=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\win32\conproc.h"\
	".\win32\winquake.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_SYS_W=\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\win32\conproc.h"\
	".\win32\winquake.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\win32\vid_dll.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_VID_D=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\win32\winquake.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_VID_D=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	".\win32\winquake.h"\
	

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\client\x86.c

!IF  "$(CFG)" == "ufo - Win32 Release"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug"

!ELSEIF  "$(CFG)" == "ufo - Win32 Debug Alpha"

DEP_CPP_X86_C=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ELSEIF  "$(CFG)" == "ufo - Win32 Release Alpha"

DEP_CPP_X86_C=\
	".\client\cdaudio.h"\
	".\client\client.h"\
	".\client\console.h"\
	".\client\input.h"\
	".\client\keys.h"\
	".\client\ref.h"\
	".\client\screen.h"\
	".\client\sound.h"\
	".\client\vid.h"\
	".\game\q_shared.h"\
	".\qcommon\qcommon.h"\
	".\qcommon\qfiles.h"\
	

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\client\anorms.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\bspfile.h
# End Source File
# Begin Source File

SOURCE=.\client\cdaudio.h
# End Source File
# Begin Source File

SOURCE=.\client\cl_basemanagement.h
# End Source File
# Begin Source File

SOURCE=.\client\cl_research.h
# End Source File
# Begin Source File

SOURCE=.\client\client.h
# End Source File
# Begin Source File

SOURCE=.\win32\conproc.h
# End Source File
# Begin Source File

SOURCE=.\client\console.h
# End Source File
# Begin Source File

SOURCE=.\game\game.h
# End Source File
# Begin Source File

SOURCE=.\client\input.h
# End Source File
# Begin Source File

SOURCE=.\client\keys.h
# End Source File
# Begin Source File

SOURCE=.\game\q_shared.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\qcommon.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\qfiles.h
# End Source File
# Begin Source File

SOURCE=.\client\ref.h
# End Source File
# Begin Source File

SOURCE=.\client\screen.h
# End Source File
# Begin Source File

SOURCE=.\server\server.h
# End Source File
# Begin Source File

SOURCE=.\client\snd_loc.h
# End Source File
# Begin Source File

SOURCE=.\client\sound.h
# End Source File
# Begin Source File

SOURCE=.\client\vid.h
# End Source File
# Begin Source File

SOURCE=.\win32\winquake.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\win32\q2.ico
# End Source File
# Begin Source File

SOURCE=.\win32\q2.rc
# End Source File
# Begin Source File

SOURCE=.\win32\ufo.ico
# End Source File
# Begin Source File

SOURCE=.\win32\ogg.lib
# End Source File
# Begin Source File

SOURCE=.\win32\vorbisfile.lib
# End Source File
# Begin Source File

SOURCE=.\win32\intl.lib
# End Source File
# End Group
# Begin Source File

SOURCE=.\win32\libintl.h
# End Source File
# Begin Source File

SOURCE=.\win32\vorbisfile.h
# End Source File
# End Target
# End Project
