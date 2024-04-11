AC_PREREQ(2.60)
AC_INIT
AC_ARG_ENABLE(depackers, [  --disable-depackers     Don't build depackers])
AC_ARG_ENABLE(prowizard, [  --disable-prowizard     Don't build ProWizard])
AC_ARG_ENABLE(static,    [  --enable-static         Build static library])
AC_ARG_ENABLE(shared,    [  --disable-shared        Don't build shared library])
AC_SUBST(LD_VERSCRIPT)
AC_SUBST(LIBM)
AC_SUBST(DARWIN_VERSION)
AC_CANONICAL_HOST
AC_PROG_CC
LT_PATH_LD
AC_CHECK_TOOL(AR, [ar], [:])
AC_PROG_RANLIB
AC_PROG_INSTALL

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
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [[
    #ifdef $1
    int ok;
    #else
    choke me
    #endif
  ]])],[AS_VAR_SET(ac_var, yes)],[AS_VAR_SET(ac_var, no)]))
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

AC_CHECK_DEFINED(__clang__)

case "${host_os}" in
dnl Skip this on platforms where it is just simply busted.
 openbsd*) ;;
 darwin*) LDFLAGS="$LDFLAGS -Wl,-undefined,error" ;;
 dnl For whatever reason, the Clang sanitizers and --no-undefined for
 dnl shared libraries are incompatible.
 *) if test "$ac_cv_defined___clang__" = "no" || test "${LDFLAGS#*fsanitize}" = "$LDFLAGS"
    then
      save_LDFLAGS="$LDFLAGS"
      LDFLAGS="$LDFLAGS -Wl,--no-undefined"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])], [], [LDFLAGS="$save_LDFLAGS"])
    fi
    ;;
esac

if test "${enable_static}" = yes; then
  AC_SUBST(STATIC,static)
fi
if test "${enable_shared}" != no; then
  AC_CHECK_DEFINED(_WIN32,[
    AC_SUBST(SHARED,dll)
    CFLAGS_SHARED="-DBUILDING_DLL"
  ],[
    CFLAGS_SHARED="-fPIC"
    AC_CHECK_DEFINED(__APPLE__,[
      AC_SUBST(SHARED,dylib)
    ],[
      AC_SUBST(SHARED,shared)
    ])
  ])
  AC_SUBST(CFLAGS_SHARED)
fi

XMP_TRY_COMPILE(whether compiler understands -Wall,
  ac_cv_c_flag_w_all,
  -Wall,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Wall")

XMP_TRY_COMPILE(whether compiler understands -Wextra,
  ac_cv_c_flag_w_extra,
  -Wextra,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Wextra")

if test x$ac_cv_c_flag_w_extra != xyes; then
  XMP_TRY_COMPILE(whether compiler understands -W,
    ac_cv_c_flag_w, -W,
   [int main(void){return 0;}], CFLAGS="${CFLAGS} -W")
fi

XMP_TRY_COMPILE(whether compiler understands -Wunused-parameter,
  ac_cv_c_flag_w_unused_parameter,
  -Wunused-parameter,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Wno-unused-parameter")

XMP_TRY_COMPILE(whether compiler understands -Wsign-compare,
  ac_cv_c_flag_w_sign_compare,
  -Wsign-compare,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Wno-sign-compare")

XMP_TRY_COMPILE(whether compiler understands -Wwrite-strings,
  ac_cv_c_flag_w_write_strings,
  -Wwrite-strings,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Wwrite-strings")

old_CFLAGS="${CFLAGS}"
XMP_TRY_COMPILE(whether compiler understands -Werror,
  ac_cv_c_flag_w_error,
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
__attribute__((visibility("default"))) int foo(void);
__attribute__((visibility("hidden")))  int bar(void);
int foo(void) { return 0; }
int bar(void) { return 1; }])]
)

case $host_os in
    mingw*|cygwin*|emx*|*djgpp|amigaos*|aros*|morphos*)
	if test $ac_cv_c_flag_f_visibility_hidden = yes; then
	   AC_MSG_NOTICE([ignoring -fvisibility flags for $host_os])
	fi
	ac_cv_c_flag_f_visibility_hidden=no
	;;
esac

have_versioned_symbols=no
if test $ac_cv_c_flag_f_visibility_hidden = no; then
   CFLAGS="${old_CFLAGS}"
else
   CFLAGS="${old_CFLAGS} -fvisibility=hidden -DXMP_SYM_VISIBILITY"

   if test $lt_cv_prog_gnu_ld = yes; then
      case "${host_os}" in
        emscripten*|beos*|atheos*|*mint)
          ;;
        *) if test "${enable_shared}" != no; then
              have_versioned_symbols=yes
              CFLAGS_SHARED="${CFLAGS_SHARED} -DUSE_VERSIONED_SYMBOLS=1"
         fi
         LD_VERSCRIPT="-Wl,--version-script,libxmp.map"
         ;;
      esac
   fi

   old_CFLAGS="${CFLAGS}"
   if test $ac_cv_c_flag_w_error = yes; then
      CFLAGS="${CFLAGS} -Werror"
   fi
   XMP_TRY_COMPILE(whether compiler understands attribute externally_visible,
    ac_cv_c_attribute_visibility_external,[],[
__attribute__((visibility("default"),externally_visible)) int foo(void);
int foo(void) { return 0; }],
    [CFLAGS="${old_CFLAGS} -DHAVE_EXTERNAL_VISIBILITY"],
    [CFLAGS="${old_CFLAGS}"])

   XMP_TRY_COMPILE(whether compiler understands attribute __symver__,
    ac_cv_c_attribute_symver,[-Werror=attributes],[
void foo(void) __attribute__((__symver__("foo@bar")));],
    [CFLAGS="${CFLAGS} -DHAVE_ATTRIBUTE_SYMVER"],
    [CFLAGS="${CFLAGS}"])
fi

AC_MSG_CHECKING(whether to enable versioned symbols support)
AC_MSG_RESULT($have_versioned_symbols)

dnl for clang
XMP_TRY_COMPILE(whether compiler understands -Wunknown-warning-option,
  ac_cv_c_flag_w_unknown_warning_option,
  -Wunknown-warning-option,[
  int main(void){return 0;}],
  CFLAGS="${CFLAGS} -Wno-unknown-warning-option")

if test "${enable_depackers}" != no; then
  DEPACKER_OBJS='$(DEPACKER_OBJS)'
else
  CFLAGS="${CFLAGS} -DLIBXMP_NO_DEPACKERS"
fi
AC_SUBST(DEPACKER_OBJS)

if test "${enable_prowizard}" != no; then
  PROWIZARD_OBJS='$(PROWIZARD_OBJS)'
else
  CFLAGS="${CFLAGS} -DLIBXMP_NO_PROWIZARD"
fi
AC_SUBST(PROWIZARD_OBJS)

XMP_TRY_COMPILE(whether alloca() needs alloca.h,
  ac_cv_c_flag_w_have_alloca_h,,[
  #include <alloca.h>
  int main(void){return 0;}],
  AC_DEFINE(HAVE_ALLOCA_H, 1, [ ]))

LIBM=
case "${host_os}" in
dnl These systems don't have libm or don't need it (list based on libtool)
mingw*|darwin*|cygwin*|haiku*|beos*|cegcc*|pw32*)
  ;;
dnl djgpp has all c89 math funcs in libc.a
*djgpp)
  ;;
*) AC_CHECK_LIB(m, ceil, LIBM="-lm")
  LIBS="${LIBS} ${LIBM}"
  ;;
esac
AC_CHECK_FUNCS(powf)

AC_MSG_CHECKING(for dirent)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <dirent.h>]], [[
  DIR *dirp = opendir(".");
  struct dirent *d = readdir(dirp);
  (void)d; closedir(dirp);]])],
 [have_dirent=yes
  AC_DEFINE(HAVE_DIRENT,1,[])],
 [have_dirent=no])
AC_MSG_RESULT($have_dirent)

AC_CHECK_FUNCS(popen mkstemp fnmatch umask)
dnl fork, execv & co don't work with djgpp
case "${host_os}" in
*djgpp|mingw*|riscos*)
  ;;
*) AC_CHECK_FUNCS(wait pipe fork execvp dup2)
  ;;
esac

case "${host_os}" in
amigaos*|aros*|morphos*)
dnl for depackers/xfd.c
  AC_CHECK_HEADERS(proto/xfdmaster.h)
  ;;
esac

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([libxmp.pc])
AC_OUTPUT
