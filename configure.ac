dnl AC_CONFIG_AUX_DIR(./scripts)
AC_INIT
0>confdefs.h
AC_ARG_ENABLE(static, [  --disable-static        Don't build static library])
AC_ARG_ENABLE(shared, [  --disable-shared        Don't build shared library])
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB

dnl XMP_TRY_COMPILE(<message>,<cache-var>,<flags>,<program>,<ifyes>,<ifno>)
AC_DEFUN([XMP_TRY_COMPILE],[
  AC_CACHE_CHECK([$1],[$2],[
    oldcflags="${CFLAGS}"
    CFLAGS="${CFLAGS} $3"
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[$4]])],[$2=yes],[$2=no],[true])
    CFLAGS="${oldcflags}"])
  AS_IF([test "x$$2" = xyes], [$5], [$6])])

AC_DEFUN([AC_CHECK_DEFINED],[
  AS_VAR_PUSHDEF([ac_var],[ac_cv_defined_$1])dnl
  AC_CACHE_CHECK([for $1 defined], ac_var,
  AC_TRY_COMPILE(,[
    #ifdef $1
    int ok;
    #else
    choke me
    #endif
  ],AS_VAR_SET(ac_var, yes),AS_VAR_SET(ac_var, no)))
  AS_IF([test AS_VAR_GET(ac_var) != "no"], [$2], [$3])dnl
  AS_VAR_POPDEF([ac_var])dnl
])

if test "${enable_static}" != no; then
  AC_SUBST(STATIC,static)
fi
if test "${enable_shared}" != no; then
  AC_CHECK_DEFINED(_WIN32,[
    AC_SUBST(SHARED,dll)
    CFLAGS="${CFLAGS} -DBUILDING_DLL"
  ],[
    AC_SUBST(SHARED,shared)
  ])
fi

XMP_TRY_COMPILE(whether compiler understands -fvisibility=hidden,
    ac_cv_c_flag_f_visibility_hidden,
    -fvisibility=hidden,[
    int main(){}],
    CFLAGS="${CFLAGS} -fvisibility=hidden")  

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

AC_CHECK_LIB(m,pow)
AC_CHECK_FUNCS(popen mkstemp fnmatch strlcpy)
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([libxmp.pc])
AC_OUTPUT
