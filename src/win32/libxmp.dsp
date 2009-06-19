# Microsoft Developer Studio Project File - Name="libxmp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libxmp - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "libxmp.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "libxmp.mak" CFG="libxmp - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "libxmp - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libxmp - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libxmp - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ""
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../win32" /I "include" /I "../include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x410 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libxmp - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../win32" /I "include" /I "../include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x410 /d "_DEBUG"
# ADD RSC /l 0x410 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF

# Begin Target

# Name "libxmp - Win32 Release"
# Name "libxmp - Win32 Debug"
# Begin Group "Loaders"

# PROP Default_Filter ""
# Begin Group "Loaders Sources"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\loaders\669_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\alm_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\amd_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\amf_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\arch_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\asif.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\common.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\dbm_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\digi_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\dmf_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\dt_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\dtt_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\emod_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\far_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\flt_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\fnk_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\gdm_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\gtk_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\hsc_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\ice_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\iff.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\imf_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\ims_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\it_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\itsex.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\liq_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\mdl_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\med3_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\med4_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\mgt_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\mmd1_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\mmd3_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\mod_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\mtm_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\no_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\okt_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\psm_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\pt3_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\ptm_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\rad_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\rtm_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\s3m_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\sfx_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\ssmt_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\st_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\stim_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\stm_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\stx_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\svb_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\sym_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\tcb_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\ult_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\umx_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# Begin Source File

SOURCE=..\loaders\xm_load.c
# ADD CPP /I "../loaders/include"
# SUBTRACT CPP /I "include"
# End Source File
# End Group
# Begin Group "Loaders Headers"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\loaders\include\asif.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\far.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\iff.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\imf.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\it.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\liq.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\load.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\med.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\mod.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\mtm.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\ptm.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\rtm.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\s3m.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\stm.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\stx.h
# End Source File
# Begin Source File

SOURCE=..\loaders\include\xm.h
# End Source File
# End Group
# End Group
# Begin Group "Player"

# PROP Default_Filter ""
# Begin Group "Player Sources"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\player\convert.c
# End Source File
# Begin Source File

SOURCE=..\player\cruncher.c
# End Source File
# Begin Source File

SOURCE=..\player\driver.c
# End Source File
# Begin Source File

SOURCE=..\player\filter.c
# End Source File
# Begin Source File

SOURCE=..\player\fmopl.c
# End Source File
# Begin Source File

SOURCE=..\player\formats.c
# End Source File
# Begin Source File

SOURCE=..\player\med_synth.c
# End Source File
# Begin Source File

SOURCE=..\player\misc.c
# End Source File
# Begin Source File

SOURCE=..\player\mix_all.c
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

SOURCE=..\player\synth.c
# End Source File
# Begin Source File

SOURCE=..\player\ulaw.c
# End Source File
# Begin Source File

SOURCE=..\player\version.c
# End Source File
# End Group
# Begin Group "Player Headers"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\include\convert.h
# End Source File
# Begin Source File

SOURCE=..\include\driver.h
# End Source File
# Begin Source File

SOURCE=..\include\effects.h
# End Source File
# Begin Source File

SOURCE=..\player\fmopl.h
# End Source File
# Begin Source File

SOURCE=..\include\mixer.h
# End Source File
# Begin Source File

SOURCE=..\include\period.h
# End Source File
# Begin Source File

SOURCE=..\player\player.h
# End Source File
# Begin Source File

SOURCE=..\player\synth.h
# End Source File
# End Group
# End Group
# Begin Group "Drivers"

# PROP Default_Filter ""
# Begin Group "Drivers Sources"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\drivers\callback.c
# End Source File
# Begin Source File

SOURCE=..\drivers\file.c
# End Source File
# Begin Source File

SOURCE=..\drivers\wav.c
# End Source File
# Begin Source File

SOURCE=..\drivers\win32.c
# End Source File
# End Group
# Begin Group "Drivers Headers"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Group
# Begin Group "Prowizard"

# PROP Default_Filter ""
# Begin Group "Prowiz Sources"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
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

SOURCE="..\loaders\prowizard\fc-m.c"
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

SOURCE=..\loaders\prowizard\p40.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\p60a.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\pha.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\pm10c.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\pm18a.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\prowiz.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\prun1.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\prun2.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\ptktable.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\tdd.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\tp3.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\tuning.c
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

SOURCE=..\loaders\prowizard\xann.c
# End Source File
# Begin Source File

SOURCE=..\loaders\prowizard\zen.c
# End Source File
# End Group
# Begin Group "Prowiz Headers"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\loaders\prowizard\prowiz.h
# End Source File
# End Group
# End Group
# Begin Group "Misc"

# PROP Default_Filter ""
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\misc\arcfs.c
# End Source File
# Begin Source File

SOURCE=..\misc\control.c
# End Source File
# Begin Source File

SOURCE=.\debug.c
# End Source File
# Begin Source File

SOURCE=..\misc\info.c
# End Source File
# Begin Source File

SOURCE=..\misc\load.c
# End Source File
# Begin Source File

SOURCE=..\misc\mkstemp.c
# End Source File
# Begin Source File

SOURCE=..\misc\mmcmp.c
# End Source File
# Begin Source File

SOURCE=..\misc\oxm.c
# End Source File
# Begin Source File

SOURCE=..\misc\ppdepack.c
# End Source File
# Begin Source File

SOURCE=.\ptpopen.c
# End Source File
# Begin Source File

SOURCE=..\misc\readdata.c
# End Source File
# Begin Source File

SOURCE=..\misc\readhuff.c
# End Source File
# Begin Source File

SOURCE=..\misc\readlzw.c
# End Source File
# Begin Source File

SOURCE=..\misc\readrle.c
# End Source File
# Begin Source File

SOURCE=..\misc\unarc.c
# End Source File
# Begin Source File

SOURCE=..\misc\unsqsh.c
# End Source File
# Begin Source File

SOURCE=.\usleep.c
# End Source File
# Begin Source File

SOURCE=..\misc\xfd.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=..\include\getopt.h
# End Source File
# Begin Source File

SOURCE=..\include\list.h
# End Source File
# Begin Source File

SOURCE=..\include\loader.h
# End Source File
# Begin Source File

SOURCE=.\osdcomm.h
# End Source File
# Begin Source File

SOURCE=..\include\readhuff.h
# End Source File
# Begin Source File

SOURCE=..\include\readlzw.h
# End Source File
# Begin Source File

SOURCE=..\include\readrle.h
# End Source File
# Begin Source File

SOURCE=..\include\xmp.h
# End Source File
# Begin Source File

SOURCE=..\include\xmpi.h
# End Source File
# Begin Source File

SOURCE=..\include\xxm.h
# End Source File
# End Group
# End Group
# End Target
# End Project
