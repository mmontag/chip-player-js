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

dnl XMP_TRY_COMPILE(<message>,<cache-var>,<flags>,<program>,<ifyes>,<ifno>)
define(XMP_TRY_COMPILE,[
  AC_CACHE_CHECK([$1],[$2],[
    oldcflags="${CFLAGS}"
    CFLAGS="${CFLAGS} $3"
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[$4]])],[$2=yes],[$2=no],[true])
    CFLAGS="${oldcflags}"])
  if test "x$$2" = xyes; then
    true; $5
  else
    true; $6
  fi])

if test "${GCC-no}" = yes; then
  XMP_TRY_COMPILE(whether compiler understands -Wunused-but-set-variable,
    ac_cv_c_flag_w_unused_but_set_variable,
    -Wunused-but-set-variable,[
    int main(){}],
    CFLAGS="${CFLAGS} -Wno-unused-but-set-variable")  

  XMP_TRY_COMPILE(whether compiler understands -Warray-bounds,
    ac_cv_c_flag_w_array_bounds,
    -Warray-bounds,[
    int main(){}],
    CFLAGS="${CFLAGS} -Wno-array-bounds")  
fi

AC_CHECK_LIB(m,pow)
AC_CHECK_FUNCS(popen mkstemp fnmatch strlcpy)
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([libxmp.pc])
AC_OUTPUT
