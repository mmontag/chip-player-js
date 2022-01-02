AC_INIT
0>confdefs.h
AC_PROG_CC
AC_C_BIGENDIAN

dnl XMP_TRY_COMPILE(<message>,<cache-var>,<flags>,<program>,<ifyes>,<ifno>)
AC_DEFUN([XMP_TRY_COMPILE],[
  AC_CACHE_CHECK([$1],[$2],[
    oldcflags="${CFLAGS}"
    CFLAGS="${CFLAGS} $3"
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[$4]])],[$2=yes],[$2=no],[true])
    CFLAGS="${oldcflags}"])
  AS_IF([test "x$$2" = xyes], [$5], [$6])])

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

AC_CHECK_LIB(m,pow)
AC_CHECK_FUNCS(pipe popen mkstemp fnmatch)
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
