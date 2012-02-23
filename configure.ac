dnl AC_CONFIG_AUX_DIR(./scripts)
AC_INIT
0>confdefs.h
AC_ARG_ENABLE(static, [  --disable-static        Don't build static library])
AC_ARG_ENABLE(shared, [  --disable-shared        Don't build shared library])
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
if test "${enable_static}" != no; then
  AC_SUBST(STATIC,static)
fi
if test "${enable_shared}" != no; then
  AC_SUBST(SHARED,shared)
fi
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([libxmp.pc])
AC_OUTPUT
