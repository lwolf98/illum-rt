AC_INIT([rtgi], [2021], [kai selgrad at oth regensburg de])

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

have_gl=yes
AC_CHECK_LIB([OpenGL], [main], [], [have_gl=no])
AC_CHECK_LIB([GLEW], [main], [], [have_gl=no])
AC_CHECK_LIB([glfw], [main], [], [have_gl=no])
AM_CONDITIONAL([have_gl], [test x$have_gl = xyes])
AS_IF([test x$have_gl = xyes],
	  [AC_DEFINE([HAVE_GL], [1], [Do we have OpenGL])],
	  [AC_MSG_WARN([Compiling without OpenGL support])])

## support older systems with obsolete imagemagick version
AC_MSG_CHECKING([MagickWand version])
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

## g++-8 filesystem troubles
## earlier versions are not supported
AC_MSG_CHECKING([whether g++ needs libstdc++fs])
case "$(g++ --version | head -n1)" in
*8.?.?)	LIBS="$LIBS -lstdc++fs"
	AC_MSG_RESULT([yes, sadly])
	;;
*)
	AC_MSG_RESULT([no, all is well])
esac

AC_CONFIG_FILES([Makefile 
				driver/Makefile
				libgi/Makefile
				rt/Makefile
				rt/seq/Makefile
				gi/Makefile
				scripts/Makefile])

#ifndef RTGI_SKIP_BVH
AC_CONFIG_FILES([rt/bbvh-base/Makefile])
#endif
AM_COND_IF([have_gl], [AC_CONFIG_FILES([rt/gl/Makefile])])

AC_OUTPUT
