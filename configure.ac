AC_INIT([rtgi], [2020], [kai selgrad at oth regensburg de])

AC_CONFIG_SRCDIR([driver/main.cpp])
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_AUX_DIR([auxx])

AM_INIT_AUTOMAKE()

AC_PROG_CXX
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX(17)
AC_PROG_RANLIB
AC_OPENMP
CXXFLAGS="$CXXFLAGS $OPENMP_CXXFLAGS"

AC_CHECK_HEADER([glm/glm.hpp], [], [AC_MSG_ERROR([You need to install glm. On Debian-style distros this is libglm-dev.])])
AC_CHECK_HEADER([png++/png.hpp],[],[AC_MSG_ERROR([You need to install png++. On Debian-style distros this is libpng++-dev])])
AC_CHECK_LIB([png], [main], [], [AC_MSG_ERROR([You need to install libpng (should be pulled in by png++)])])
AC_CHECK_LIB([assimp], [main], [], [AC_MSG_ERROR([You need to install assimp. On Debian-style distros this is libassimp-dev])])


AC_MSG_CHECKING([MagickWand])
if pkg-config --atleast-version=7.0.0 MagickWand ; then
	AC_MSG_RESULT([v7])
	WAND_CFLAGS="$(pkg-config MagickWand --cflags) -D RTGI_WAND7"
	WAND_LIBS="$(pkg-config MagickWand --libs)"
else 
	if pkg-config --atleast-version=6.0.0 MagickWand ; then
		AC_MSG_RESULT([v6])
		WAND_CFLAGS="$(pkg-config MagickWand --cflags) -D RTGI_WAND6"
		WAND_LIBS="$(pkg-config MagickWand --libs)"
	else
		AC_MSG_FAILURE([Requires MagickWand v6 or v7])
	fi
fi
AC_SUBST([WAND_CFLAGS])
AC_SUBST([WAND_LIBS])


AC_CONFIG_FILES([Makefile 
				driver/Makefile
				libgi/Makefile
				rt/Makefile rt/bbvh-base/Makefile
				gi/Makefile])

AC_OUTPUT
