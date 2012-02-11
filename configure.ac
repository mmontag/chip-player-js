dnl AC_CONFIG_AUX_DIR(./scripts)
AC_INIT
0>confdefs.h
AC_ARG_ENABLE(static, [  --enable-static         Build static library])
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB

if test "${enable_static}"; then
  AC_SUBST(STATIC,static)
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
