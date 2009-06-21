
/* Define DRIVER_OSS* to enable OSS /dev/sequencer and /dev/dsp support */
#cmakedefine DRIVER_OSS
#cmakedefine DRIVER_OSS_SEQ

/* Define to enable OSX CoreAudio support */
#cmakedefine DRIVER_OSX

/* Define to enable HP-UX audio support */
#cmakedefine DRIVER_HPUX

/* Define to enable Solaris and S/Linux audio support */
#cmakedefine DRIVER_SOLARIS

/* Define to enable SGI audio support */
#cmakedefine DRIVER_SGI

/* Define to enable NetBSD audio support */
#cmakedefine DRIVER_NETBSD

/* Define to enable OpenBSD audio support */
#cmakedefine DRIVER_OPENBSD

/* Define to enable BSD audio support */
#cmakedefine DRIVER_BSD

/* Define to enable QNX audio support */
#cmakedefine DRIVER_QNX

/* Define to enable BeOS audio support */
#cmakedefine DRIVER_BEOS

/* Define to enable Amiga AHI audio support */
#cmakedefine DRIVER_AMIGA

/* Define to enable Win32 audio support */
#cmakedefine DRIVER_WIN32

/* Define to enable ESD (Enlghtened Sound Daemon) support */
#cmakedefine DRIVER_ESD

/* Define to enable aRts (KDE sound server) support */
#cmakedefine DRIVER_ARTS

/* Define to enable NAS (Network Audio System) support */
#cmakedefine DRIVER_NAS

/* Define to enable PulseAudio support */
#cmakedefine DRIVER_PULSEAUDIO

/* Define to enable ALSA support */
#cmakedefine DRIVER_ALSA_SEQ
#cmakedefine DRIVER_ALSA

/* Define to enable ALSA 0.5 support */
#cmakedefine DRIVER_ALSA05

/* Define if you don't have vprintf but do have _doprnt.  */
#cmakedefine HAVE_DOPRNT

/* Define if OSS defines audio_buf_info.  */
#cmakedefine HAVE_AUDIO_BUF_INFO

/* Define if awedrv defines AWE_MD_NEW_VOLUME_CALC.  */
#cmakedefine HAVE_AWE_MD_NEW_VOLUME_CALC

/* Define if you have the vprintf function.  */
#cmakedefine HAVE_VPRINTF

/* Define if you have the snprintf function.  */
#cmakedefine HAVE_SNPRINTF

/* Define if you have the usleep function.  */
#cmakedefine HAVE_USLEEP

/* Define if you have the popen function.  */
#cmakedefine HAVE_POPEN

/* Define if you have the gettimeofday function.  */
#cmakedefine HAVE_GETTIMEOFDAY

/* Define if you have the getopt_long function.  */
#cmakedefine HAVE_GETOPT_LONG
#ifdef HAVE_GETOPT_LONG
#define ELIDE_CODE
#endif

/* Define as the return type of signal handlers (int or void).  */
#cmakedefine RETSIGTYPE

/* Define if you have the ANSI C header files.  */
#cmakedefine STDC_HEADERS

/* Define if you have the select function.  */
#cmakedefine HAVE_SELECT

/* Define if you have the <sys/select.h> header file.  */
#cmakedefine HAVE_SYS_SELECT_H

/* Define if you have the <signal.h> header file.  */
#cmakedefine HAVE_SIGNAL_H

/* Define if you have the <unistd.h> header file.  */
#cmakedefine HAVE_UNISTD_H

/* Define if you have the <termios.h> header file.  */
#cmakedefine HAVE_TERMIOS_H

/* Define if you have the <sys/shm.h> header file.  */
#cmakedefine HAVE_SYS_SHM_H

/* Define if you have the <sbus/audio/audio.h> header file.  */
#cmakedefine HAVE_SBUS_AUDIO_AUDIO_H

/* Define if you have the <awe_voice.h> header file.  */
#cmakedefine HAVE_AWE_VOICE_H

/* Define if you have the <linux/awe_voice.h> header file.  */
#cmakedefine HAVE_LINUX_AWE_VOICE_H

/* Define if you have the <machine/soundcard.h> header file.  */
#cmakedefine HAVE_MACHINE_SOUNDCARD_H

/* Define if you have the <linux/ultrasound.h> header file.  */
#cmakedefine HAVE_LINUX_ULTRASOUND_H

/* Define if you have the <machine/ultrasound.h> header file.  */
#cmakedefine HAVE_MACHINE_ULTRASOUND_H

/* Define if you have the <sys/audioio.h> header file.  */
#cmakedefine HAVE_SYS_AUDIOIO_H

/* Define if you have the <sys/audio.io.h> header file.  */
#cmakedefine HAVE_SYS_AUDIO_IO_H

/* Define if you have the <sun/audioio.h> header file.  */
#cmakedefine HAVE_SUN_AUDIOIO_H

/* Define if you have the <sys/asoundlib.h> header file.  */
#cmakedefine HAVE_SYS_ASOUNDLIB_H

/* Define if you have the <strings.h> header file.  */
#cmakedefine HAVE_STRINGS_H

/* Define if you have the <getopt.h> header file.  */
#cmakedefine HAVE_GETOPT_H

/* Define if you have the <sys/awe_voice.h> header file.  */
#cmakedefine HAVE_SYS_AWE_VOICE_H

/* Define if you have the <sys/select.h> header file.  */
#cmakedefine HAVE_SYS_SELECT_H

/* Define if you have the <sys/param.h> header file.  */
#cmakedefine HAVE_SYS_PARAM_H

/* Define if you have the <sys/soundcard.h> header file.  */
#cmakedefine HAVE_SYS_SOUNDCARD_H

/* Define if you have the <sys/ultrasound.h> header file.  */
#cmakedefine HAVE_SYS_ULTRASOUND_H

/* Define if you have the <sys/resource.h> header file.  */
#cmakedefine HAVE_SYS_RESOURCE_H

/* Define if you have the <sys/rtprio.h> header file.  */
#cmakedefine HAVE_SYS_RTPRIO_H

/* End of config.h.in */
