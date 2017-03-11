dnl AC_CONFIG_AUX_DIR(./scripts)
AC_INIT
AC_ARG_ENABLE(it,     [  --disable-it            Don't build IT format support])
AC_ARG_ENABLE(static, [  --enable-static         Build static library])
AC_ARG_ENABLE(shared, [  --disable-shared        Don't build shared library])
AC_SUBST(LD_VERSCRIPT)
AC_SUBST(DARWIN_VERSION)
AC_CANONICAL_HOST
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_C_BIGENDIAN

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

case "${host_cpu}" in
powerpc64)
  CFLAGS="${CFLAGS} -m64"
  LDFLAGS="${LDFLAGS} -m64"
  ;;
esac

case "${host_os}" in
darwin*)
  DARWIN_VERSION="`echo ${host_os#darwin}|cut -f1 -d'.'`"
  if test "${enable_shared}" != no; then
    CFLAGS="${CFLAGS} -fno-common"
  fi
  ;;
cygwin* | *djgpp | mint* | amigaos* |aros* | morphos*)
  enable_static=yes
  enable_shared=no
  ;;
esac

if test "${enable_static}" = yes; then
  AC_SUBST(STATIC,static)
fi
if test "${enable_shared}" != no; then
  AC_CHECK_DEFINED(_WIN32,[
    AC_SUBST(SHARED,dll)
    CFLAGS="${CFLAGS} -DBUILDING_DLL"
  ],[
    AC_CHECK_DEFINED(__APPLE__,[
      AC_SUBST(SHARED,dylib)
    ],[
      AC_SUBST(SHARED,shared)
    ])
  ])
fi

if test "${enable_it}" = no; then
  CFLAGS="${CFLAGS} -DLIBXMP_CORE_DISABLE_IT"
fi

XMP_TRY_COMPILE(whether compiler understands -Wall,
  ac_cv_c_flag_w_all,
  -Wall,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Wall")  

old_CFLAGS="${CFLAGS}"
XMP_TRY_COMPILE(whether compiler understands -Werror,
  ac_cv_c_flag_w_all,
  -Werror,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Werror")

XMP_TRY_COMPILE(whether compiler understands -xldscope=hidden,
  ac_cv_c_flag_xldscope_hidden,
  -xldscope=hidden,[
  __global int foo(void) {}],
  [CFLAGS="${old_CFLAGS} -xldscope=hidden -DXMP_LDSCOPE_GLOBAL"],
  [XMP_TRY_COMPILE(whether compiler understands -fvisibility=hidden,
    ac_cv_c_flag_f_visibility_hidden,
    -fvisibility=hidden,[
    __attribute__((visibility("default"))) void foo(void) {}],
    [CFLAGS="${old_CFLAGS} -fvisibility=hidden -DXMP_SYM_VISIBILITY"
    LD_VERSCRIPT="-Wl,--version-script,libxmp.map"],
    [CFLAGS="${old_CFLAGS}"])]
  )
dnl we want symbol -fvisibility for elf targets, however it works
dnl with darwin/macho too.  other than that, windows, dos, os/2,
dnl amiga do not want it:  for any such targets, the -Werror switch
dnl is should fail the above check, but it does not.  adding the
dnl manual test below for that, instead.
case $host_os in
    mingw*|cygwin*|emx*|*djgpp|amigaos*|aros*|morphos*)
	CFLAGS="${old_CFLAGS}"
	if test $ac_cv_c_flag_f_visibility_hidden = yes; then
	  AC_MSG_NOTICE([ignoring -fvisibility flags for $host_os])
	fi
	;;
esac

dnl for clang
XMP_TRY_COMPILE(whether compiler understands -Wunknown-warning-option,
  ac_cv_c_flag_w_unknown_warning_option,
  -Wunknown-warning-option,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Wno-unknown-warning-option")  

XMP_TRY_COMPILE(whether compiler understands -Wunused-but-set-variable,
  ac_cv_c_flag_w_unused_but_set_variable,
  -Wunused-but-set-variable,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Wno-unused-but-set-variable")  

XMP_TRY_COMPILE(whether compiler understands -Wunused-result,
  ac_cv_c_flag_w_unused_result,
  -Wunused-result,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Wno-unused-result")  

XMP_TRY_COMPILE(whether compiler understands -Warray-bounds,
  ac_cv_c_flag_w_array_bounds,
  -Warray-bounds,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Wno-array-bounds")  

old_LIBS="${LIBS}"
AC_CHECK_LIB(m,pow)
dnl -lm not needed with darwin.
case "${host_os}" in
darwin*)
  LIBS="${old_LIBS}"
  ;;
esac
AC_CHECK_FUNCS(localtime_r round powf)

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([libxmp-lite.pc])
AC_OUTPUT
