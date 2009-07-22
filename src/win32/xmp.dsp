# Microsoft Developer Studio Project File - Name="xmp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=xmp - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "xmp.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "xmp.mak" CFG="xmp - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "xmp - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "xmp - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "xmp - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ""
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../win32" /I "../include" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /D "VERSION"="2.8.0-pre" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x410 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "xmp - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "xmp___Win32_Debug"
# PROP BASE Intermediate_Dir "xmp___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../win32" /I "../include" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /D "VERSION"="2.8.0-pre" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x410 /d "_DEBUG"
# ADD RSC /l 0x410 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "xmp - Win32 Release"
# Name "xmp - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"

# Begin Source File

SOURCE=..\loaders\common.c
# End Source File
# Begin Source File

SOURCE=..\loaders\iff.c
# End Source File
# Begin Source File

SOURCE=..\loaders\itsex.c
# End Source File
# Begin Source File

SOURCE=..\loaders\asif.c
# End Source File
# Begin Source File

SOURCE=..\loaders\xm_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\mod_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\s3m_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\stm_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\669_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\far_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\mtm_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\ptm_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\okt_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\alm_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\amd_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\rad_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\ult_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\mdl_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\it_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\stx_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\pt3_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\sfx_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\flt_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\st_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\emod_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\imf_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\digi_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\fnk_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\ice_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\hsc_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\liq_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\ims_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\masi_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\amf_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\psm_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\stim_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\mmd1_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\mmd3_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\rtm_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\dmf_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\tcb_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\dt_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\gtk_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\no_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\arch_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\sym_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\dtt_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\mgt_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\med3_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\med4_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\ssmt_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\dbm_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\umx_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\gdm_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\coco_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\pw_load.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\prowiz.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\ptktable.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\tuning.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\ac1d.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\di.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\eureka.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\fc-m.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\fuchs.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\fuzzac.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\gmc.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\heatseek.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\kris.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\ksm.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\mp.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\np1.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\np2.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\np3.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\p60a.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\pm10c.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\pm18a.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\pha.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\prun1.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\prun2.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\tdd.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\unic.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\unic2.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\wn.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\zen.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\tp3.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\p40.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\xann.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\p50a.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\pp21.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\starpack.c
# End Source File
# Begin Source File

SOURCE=..\misc\control.c
# End Source File
# Begin Source File

SOURCE=..\misc\info.c
# End Source File
# Begin Source File

SOURCE=..\misc\readdata.c
# End Source File
# Begin Source File

SOURCE=..\misc\load.c
# End Source File
# Begin Source File

SOURCE=..\misc\ppdepack.c
# End Source File
# Begin Source File

SOURCE=..\misc\unsqsh.c
# End Source File
# Begin Source File

SOURCE=..\misc\mmcmp.c
# End Source File
# Begin Source File

SOURCE=..\misc\readrle.c
# End Source File
# Begin Source File

SOURCE=..\misc\readhuff.c
# End Source File
# Begin Source File

SOURCE=..\misc\readlzw.c
# End Source File
# Begin Source File

SOURCE=..\misc\unarc.c
# End Source File
# Begin Source File

SOURCE=..\misc\arcfs.c
# End Source File
# Begin Source File

SOURCE=..\misc\oxm.c
# End Source File
# Begin Source File

SOURCE=..\misc\mkstemp.c
# End Source File
# Begin Source File

SOURCE=..\misc\xfd.c
# End Source File
# Begin Source File

SOURCE=..\misc\crc32.c
# End Source File
# Begin Source File

SOURCE=..\player\convert.c
# End Source File
# Begin Source File

SOURCE=..\player\driver.c
# End Source File
# Begin Source File

SOURCE=..\player\formats.c
# End Source File
# Begin Source File

SOURCE=..\player\misc.c
# End Source File
# Begin Source File

SOURCE=..\player\period.c
# End Source File
# Begin Source File

SOURCE=..\player\player.c
# End Source File
# Begin Source File

SOURCE=..\player\readrc.c
# End Source File
# Begin Source File

SOURCE=..\player\scan.c
# End Source File
# Begin Source File

SOURCE=..\player\ulaw.c
# End Source File
# Begin Source File

SOURCE=..\player\cruncher.c
# End Source File
# Begin Source File

SOURCE=..\player\med_synth.c
# End Source File
# Begin Source File

SOURCE=..\player\filter.c
# End Source File
# Begin Source File

SOURCE=..\player\fmopl.c
# End Source File
# Begin Source File

SOURCE=..\player\synth.c
# End Source File
# Begin Source File

SOURCE=..\player\mix_all.c
# End Source File
# Begin Source File

SOURCE=..\player\effects.c
# End Source File
# Begin Source File

SOURCE=..\drivers\file.c
# End Source File
# Begin Source File

SOURCE=..\drivers\wav.c
# End Source File
# Begin Source File

SOURCE=..\drivers\callback.c
# End Source File
# Begin Source File

SOURCE=..\drivers\win32.c
# End Source File
# Begin Source File

SOURCE=..\main\getopt.c
# End Source File
# Begin Source File

SOURCE=..\main\getopt1.c
# End Source File
# Begin Source File

SOURCE=..\main\options.c
# End Source File
# Begin Source File

SOURCE=..\main\main.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\config.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
