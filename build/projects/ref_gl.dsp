# Microsoft Developer Studio Project File - Name="ref_gl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=ref_gl - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ref_gl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ref_gl.mak" CFG="ref_gl - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ref_gl - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "ref_gl - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ref_gl - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\ref_gl__"
# PROP BASE Intermediate_Dir ".\ref_gl__"
# PROP BASE Target_Dir "."
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\release_refgl"
# PROP Intermediate_Dir ".\release_refgl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir "."
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /G5 /W4 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib SDL.lib SDLmain.lib SDL_ttf.lib /nologo /subsystem:windows /dll /machine:I386
# SUBTRACT LINK32 /incremental:yes /debug

!ELSEIF  "$(CFG)" == "ref_gl - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\ref_gl__"
# PROP BASE Intermediate_Dir ".\ref_gl__"
# PROP BASE Target_Dir "."
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\debug_refgl"
# PROP Intermediate_Dir ".\debug_refgl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir "."
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /G5 /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib SDL.lib SDLmain.lib SDL_ttf.lib /nologo /subsystem:windows /dll /incremental:no /map /debug /machine:I386 /out:"..\..\ref_gl.dll"
# SUBTRACT LINK32 /profile

!ENDIF 

# Begin Target

# Name "ref_gl - Win32 Release"
# Name "ref_gl - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_anim.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_arb_shader.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_bloom.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_draw.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_drawmd3.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_font.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_image.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_light.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_mesh.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_model.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_obj.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_particle.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_rmain.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_rmisc.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_rsurf.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_shadows.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_warp.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ports\win32\glw_imp.c
# End Source File
# Begin Source File

SOURCE=..\..\src\game\q_shared.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ports\win32\q_shwin.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\qgl.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ports\win32\qgl_win.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=..\..\src\client\anorms.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\anormtab.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_arb_shader.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_font.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_local.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_md3.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\gl_model.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ports\win32\glw_win.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ports\win32\jpeglib.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ports\win32\png.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ports\win32\pngconf.h
# End Source File
# Begin Source File

SOURCE=..\..\src\game\q_shared.h
# End Source File
# Begin Source File

SOURCE=..\..\src\qcommon\qcommon.h
# End Source File
# Begin Source File

SOURCE=..\..\src\qcommon\qfiles.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\qgl.h
# End Source File
# Begin Source File

SOURCE=..\..\src\client\ref.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ref_gl\warpsin.h
# End Source File
# Begin Source File

SOURCE=..\..\src\win32\winquake.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\src\ref_gl\ref_gl.def
# End Source File
# Begin Source File

SOURCE=..\..\src\ports\win32\libpng.lib
# End Source File
# Begin Source File

SOURCE=..\..\src\ports\win32\zlib1.lib
# End Source File
# Begin Source File

SOURCE=..\..\contrib\vc6\jpeg.lib
# End Source File
# End Group
# End Target
# End Project
