# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.68])
AC_INIT([vlc-libadlmidi], [1.0.0], [admin@wohlnet.ru])
AC_CONFIG_SRCDIR([libadlmidi.c])
AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS
LT_INIT

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LN_S
PKG_PROG_PKG_CONFIG

# Initialize automake stuff
AM_INIT_AUTOMAKE([foreign])

AC_C_INLINE
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRNLEN
AC_SEARCH_LIBS([sqrt], [m])
AC_CHECK_FUNCS([memset sqrt strcasecmp strcspn strdup strndup strstr strtol])

PKG_CHECK_MODULES([vlc], [vlc-plugin >= 2.0])
PKG_CHECK_MODULES([vlc212], [vlc-plugin >= 2.1])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

