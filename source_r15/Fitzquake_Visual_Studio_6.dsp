# Microsoft Developer Studio Project File - Name="Fitzquake" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=Fitzquake - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Fitzquake_Visual_Studio_6.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Fitzquake_Visual_Studio_6.mak" CFG="Fitzquake - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Fitzquake - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Fitzquake - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "Fitzquake - Win32 Release With Debug Info" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Fitzquake - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G5 /W3 /GX /O1 /I ".\dxsdk\sdk\inc" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "GLQUAKE" /Fr /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 .\dxsdk\sdk\lib\dxguid.lib advapi32.lib comctl32.lib gdi32.lib ole32.lib opengl32.lib shell32.lib user32.lib winmm.lib wsock32.lib /nologo /subsystem:windows /machine:I386 /out:"c:\Quake\fitzquake_mark_v.exe" /DELAYLOAD:libcurl.dll
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "Fitzquake - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /G5 /ML /W3 /GX /ZI /Od /I ".\dxsdk\sdk\inc" /I ".\libcurl-7.18.0-win32-msvc\include\curl" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "GLQUAKE" /D "DEBUGGL" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 .\dxsdk\sdk\lib\dxguid.lib advapi32.lib comctl32.lib gdi32.lib ole32.lib opengl32.lib shell32.lib user32.lib winmm.lib wsock32.lib /nologo /subsystem:windows /debug /machine:I386 /out:"c:\quake\fitzquake_dbg.exe" /DELAYLOAD:libcurl.dll
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "Fitzquake - Win32 Release With Debug Info"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Fitzquake___Win32_Release_With_Debug_Info"
# PROP BASE Intermediate_Dir "Fitzquake___Win32_Release_With_Debug_Info"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_With_Debug_Info"
# PROP Intermediate_Dir "Release_With_Debug_Info"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G5 /GX /Zd /Ot /Ow /Ob2 /I ".\dxsdk\sdk\inc" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "GLQUAKE" /FR /YX /FD /c
# SUBTRACT BASE CPP /Ox
# ADD CPP /nologo /G5 /GX /Zi /Ot /Ow /Ob2 /I ".\dxsdk\sdk\inc" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "GLQUAKE" /FR /YX /FD /c
# SUBTRACT CPP /Ox
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 .\dxsdk\sdk\lib\dxguid.lib comctl32.lib winmm.lib wsock32.lib opengl32.lib glu32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386 /out:"c:\Games\Quake\fitzquake.exe"
# SUBTRACT BASE LINK32 /pdb:none /debug
# ADD LINK32 .\dxsdk\sdk\lib\dxguid.lib advapi32.lib comctl32.lib gdi32.lib ole32.lib opengl32.lib shell32.lib user32.lib winmm.lib wsock32.lib /nologo /subsystem:windows /debug /machine:I386 /out:"c:\Quake\fitzquake_rel_debug.exe"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "Fitzquake - Win32 Release"
# Name "Fitzquake - Win32 Debug"
# Name "Fitzquake - Win32 Release With Debug Info"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\anorm_dots.h
# End Source File
# Begin Source File

SOURCE=.\anorms.h
# End Source File
# Begin Source File

SOURCE=.\arch_def.h
# End Source File
# Begin Source File

SOURCE=.\bspfile.h
# End Source File
# Begin Source File

SOURCE=.\cd_win.c
# End Source File
# Begin Source File

SOURCE=.\cdaudio.h
# End Source File
# Begin Source File

SOURCE=.\chase.c
# End Source File
# Begin Source File

SOURCE=.\cl_demo.c
# End Source File
# Begin Source File

SOURCE=.\cl_input.c
# End Source File
# Begin Source File

SOURCE=.\cl_main.c
# End Source File
# Begin Source File

SOURCE=.\cl_parse.c
# End Source File
# Begin Source File

SOURCE=.\cl_tent.c
# End Source File
# Begin Source File

SOURCE=.\client.h
# End Source File
# Begin Source File

SOURCE=.\cmd.c
# End Source File
# Begin Source File

SOURCE=.\cmd.h
# End Source File
# Begin Source File

SOURCE=.\common.c
# End Source File
# Begin Source File

SOURCE=.\common.h
# End Source File
# Begin Source File

SOURCE=.\common_ez.c
# End Source File
# Begin Source File

SOURCE=.\common_ez.h
# End Source File
# Begin Source File

SOURCE=.\conproc.c
# End Source File
# Begin Source File

SOURCE=.\conproc.h
# End Source File
# Begin Source File

SOURCE=.\console.c
# End Source File
# Begin Source File

SOURCE=.\console.h
# End Source File
# Begin Source File

SOURCE=.\crc.c
# End Source File
# Begin Source File

SOURCE=.\crc.h
# End Source File
# Begin Source File

SOURCE=.\cvar.c
# End Source File
# Begin Source File

SOURCE=.\cvar.h
# End Source File
# Begin Source File

SOURCE=.\downloads.c
# End Source File
# Begin Source File

SOURCE=.\downloads.h
# End Source File
# Begin Source File

SOURCE=.\draw.h
# End Source File
# Begin Source File

SOURCE=.\dshow_mp3.c
# ADD CPP /I "./dxsdk/sdk8/include"
# SUBTRACT CPP /I ".\dxsdk\sdk\inc"
# End Source File
# Begin Source File

SOURCE=.\file.c
# End Source File
# Begin Source File

SOURCE=.\file.h
# End Source File
# Begin Source File

SOURCE=.\gl_common.c
# End Source File
# Begin Source File

SOURCE=.\gl_draw.c
# End Source File
# Begin Source File

SOURCE=.\gl_fog.c
# End Source File
# Begin Source File

SOURCE=.\gl_mesh.c
# End Source File
# Begin Source File

SOURCE=.\gl_model.c
# End Source File
# Begin Source File

SOURCE=.\gl_model.h
# End Source File
# Begin Source File

SOURCE=.\gl_refrag.c
# End Source File
# Begin Source File

SOURCE=.\gl_rlight.c
# End Source File
# Begin Source File

SOURCE=.\gl_rmain.c
# End Source File
# Begin Source File

SOURCE=.\gl_rmisc.c
# End Source File
# Begin Source File

SOURCE=.\gl_screen.c
# End Source File
# Begin Source File

SOURCE=.\gl_sky.c
# End Source File
# Begin Source File

SOURCE=.\gl_test.c
# End Source File
# Begin Source File

SOURCE=.\gl_texmgr.c
# End Source File
# Begin Source File

SOURCE=.\gl_texmgr.h
# End Source File
# Begin Source File

SOURCE=.\gl_warp.c
# End Source File
# Begin Source File

SOURCE=.\gl_warp_sin.h
# End Source File
# Begin Source File

SOURCE=.\glquake.h
# End Source File
# Begin Source File

SOURCE=.\host.c
# End Source File
# Begin Source File

SOURCE=.\host_cmd.c
# End Source File
# Begin Source File

SOURCE=.\image.c
# End Source File
# Begin Source File

SOURCE=.\image.h
# End Source File
# Begin Source File

SOURCE=.\in_win.c
# End Source File
# Begin Source File

SOURCE=.\input.c
# End Source File
# Begin Source File

SOURCE=.\input.h
# End Source File
# Begin Source File

SOURCE=.\keys.c
# End Source File
# Begin Source File

SOURCE=.\keys.h
# End Source File
# Begin Source File

SOURCE=.\lists.c
# End Source File
# Begin Source File

SOURCE=.\lists.h
# End Source File
# Begin Source File

SOURCE=.\location.c
# End Source File
# Begin Source File

SOURCE=.\location.h
# End Source File
# Begin Source File

SOURCE=.\mathlib.c
# End Source File
# Begin Source File

SOURCE=.\mathlib.h
# End Source File
# Begin Source File

SOURCE=.\menu.c
# End Source File
# Begin Source File

SOURCE=.\menu.h
# End Source File
# Begin Source File

SOURCE=.\mod_load_textures.h
# End Source File
# Begin Source File

SOURCE=.\modelgen.h
# End Source File
# Begin Source File

SOURCE=.\movie.c
# End Source File
# Begin Source File

SOURCE=.\movie.h
# End Source File
# Begin Source File

SOURCE=.\net.h
# End Source File
# Begin Source File

SOURCE=.\net_defs.h
# End Source File
# Begin Source File

SOURCE=.\net_dgrm.c
# End Source File
# Begin Source File

SOURCE=.\net_dgrm.h
# End Source File
# Begin Source File

SOURCE=.\net_loop.c
# End Source File
# Begin Source File

SOURCE=.\net_loop.h
# End Source File
# Begin Source File

SOURCE=.\net_main.c
# End Source File
# Begin Source File

SOURCE=.\net_sys.h
# End Source File
# Begin Source File

SOURCE=.\net_udp.h
# End Source File
# Begin Source File

SOURCE=.\net_win.c
# End Source File
# Begin Source File

SOURCE=.\net_wins.c
# End Source File
# Begin Source File

SOURCE=.\net_wins.h
# End Source File
# Begin Source File

SOURCE=.\pr_cmds.c
# End Source File
# Begin Source File

SOURCE=.\pr_comp.h
# End Source File
# Begin Source File

SOURCE=.\pr_edict.c
# End Source File
# Begin Source File

SOURCE=.\pr_exec.c
# End Source File
# Begin Source File

SOURCE=.\progdefs.h
# End Source File
# Begin Source File

SOURCE=.\progs.h
# End Source File
# Begin Source File

SOURCE=.\protocol.h
# End Source File
# Begin Source File

SOURCE=.\q_dirent.c
# End Source File
# Begin Source File

SOURCE=.\q_dirent.h
# End Source File
# Begin Source File

SOURCE=.\q_sound.h
# End Source File
# Begin Source File

SOURCE=.\q_stdinc.h
# End Source File
# Begin Source File

SOURCE=.\quakedef.h
# End Source File
# Begin Source File

SOURCE=.\r_alias.c
# End Source File
# Begin Source File

SOURCE=.\r_brush.c
# End Source File
# Begin Source File

SOURCE=.\r_part.c
# End Source File
# Begin Source File

SOURCE=.\r_sprite.c
# End Source File
# Begin Source File

SOURCE=.\r_world.c
# End Source File
# Begin Source File

SOURCE=.\recent_file.c
# End Source File
# Begin Source File

SOURCE=.\recent_file.h
# End Source File
# Begin Source File

SOURCE=.\render.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\sbar.c
# End Source File
# Begin Source File

SOURCE=.\sbar.h
# End Source File
# Begin Source File

SOURCE=.\screen.h
# End Source File
# Begin Source File

SOURCE=.\server.h
# End Source File
# Begin Source File

SOURCE=.\snd_dma.c
# End Source File
# Begin Source File

SOURCE=.\snd_mem.c
# End Source File
# Begin Source File

SOURCE=.\snd_mix.c
# End Source File
# Begin Source File

SOURCE=.\snd_win.c
# End Source File
# Begin Source File

SOURCE=.\spritegn.h
# End Source File
# Begin Source File

SOURCE=.\strings.c
# End Source File
# Begin Source File

SOURCE=.\strings.h
# End Source File
# Begin Source File

SOURCE=.\strl_fn.h
# End Source File
# Begin Source File

SOURCE=.\strlcat.c
# End Source File
# Begin Source File

SOURCE=.\strlcpy.c
# End Source File
# Begin Source File

SOURCE=.\sv_main.c
# End Source File
# Begin Source File

SOURCE=.\sv_move.c
# End Source File
# Begin Source File

SOURCE=.\sv_phys.c
# End Source File
# Begin Source File

SOURCE=.\sv_user.c
# End Source File
# Begin Source File

SOURCE=.\sys.h
# End Source File
# Begin Source File

SOURCE=.\sys_win.c
# End Source File
# Begin Source File

SOURCE=.\sys_win_menu.c
# End Source File
# Begin Source File

SOURCE=.\sys_win_menu.h
# End Source File
# Begin Source File

SOURCE=.\sys_win_messages.c
# End Source File
# Begin Source File

SOURCE=.\sys_win_messages.h
# End Source File
# Begin Source File

SOURCE=.\sys_win_movie.c
# End Source File
# Begin Source File

SOURCE=.\sys_win_movie.h
# End Source File
# Begin Source File

SOURCE=.\talk_macro.c
# End Source File
# Begin Source File

SOURCE=.\talk_macro.h
# End Source File
# Begin Source File

SOURCE=.\tool_inspector.c
# End Source File
# Begin Source File

SOURCE=.\tool_inspector.h
# End Source File
# Begin Source File

SOURCE=.\tool_texturepointer.c
# End Source File
# Begin Source File

SOURCE=.\tool_texturepointer.h
# End Source File
# Begin Source File

SOURCE=.\unzip.cpp
# End Source File
# Begin Source File

SOURCE=.\unzip.h
# End Source File
# Begin Source File

SOURCE=.\vid.c
# End Source File
# Begin Source File

SOURCE=.\vid.h
# End Source File
# Begin Source File

SOURCE=.\vid_wgl.c
# End Source File
# Begin Source File

SOURCE=.\vid_wglext.h
# End Source File
# Begin Source File

SOURCE=.\view.c
# End Source File
# Begin Source File

SOURCE=.\view.h
# End Source File
# Begin Source File

SOURCE=.\wad.c
# End Source File
# Begin Source File

SOURCE=.\wad.h
# End Source File
# Begin Source File

SOURCE=.\winquake.h
# End Source File
# Begin Source File

SOURCE=.\world.c
# End Source File
# Begin Source File

SOURCE=.\world.h
# End Source File
# Begin Source File

SOURCE=.\wsaerror.h
# End Source File
# Begin Source File

SOURCE=.\zip.cpp
# End Source File
# Begin Source File

SOURCE=.\zip.h
# End Source File
# Begin Source File

SOURCE=.\zone.c
# End Source File
# Begin Source File

SOURCE=.\zone.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\fitzquake.ico
# End Source File
# Begin Source File

SOURCE=.\progdefs.q1
# End Source File
# Begin Source File

SOURCE=.\winquake.rc
# End Source File
# End Group
# Begin Source File

SOURCE=.\notes.txt
# End Source File
# End Target
# End Project
