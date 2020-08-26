AC_INIT([rt], [2020], [kai selgrad at oth regensburg de])

AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_AUX_DIR([auxx])

AM_INIT_AUTOMAKE()

AC_PROG_CXX
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX(11)
AC_OPENMP
CXXFLAGS="$CXXFLAGS $OPENMP_CXXFLAGS"

AC_CHECK_HEADER([glm/glm.hpp], [], [AC_MSG_ERROR([You need to install glm. On Debian-style distros this is libglm-dev.])])
AC_CHECK_HEADER([png++/png.hpp],[],[AC_MSG_ERROR([You need to install png++. On Debian-style distros this is libpng++-dev])])
AC_CHECK_LIB([png], [main], [], [AC_MSG_ERROR([You need to install libpng (should be pulled in by png++)])])
AC_CHECK_LIB([assimp], [main], [], [AC_MSG_ERROR([You need to install assimp. On Debian-style distros this is libassimp-dev])])

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
