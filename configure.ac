AC_INIT([rtgi], [2021], [kai selgrad at oth regensburg de])

AC_CONFIG_SRCDIR([driver/main.cpp])
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_AUX_DIR([auxx])

AM_INIT_AUTOMAKE([-Wno-portability])

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
# libdl is needed by OptiX
AC_CHECK_LIB([dl], [dlsym], [], [AC_MSG_ERROR([Cannot find libdl which is required for OptiX])])

have_gl=yes
AC_ARG_ENABLE([opengl], [AS_HELP_STRING([--disable-opengl], [Do not use OpenGL, even if present])], [have_gl=$enableval], [have_gl=yes])
AC_CHECK_LIB([OpenGL], [main], [], [have_gl=no])
AC_CHECK_LIB([GLEW], [main], [], [have_gl=no])
have_gl_with_glfw=$have_gl
AC_CHECK_LIB([glfw], [main], [], [have_gl_with_glfw=no])
have_headless_gl=$have_gl
AC_CHECK_LIB([EGL], [main], [], [have_headless_gl=no])
AC_CHECK_LIB([gbm], [main], [], [have_headless_gl=no])

AM_CONDITIONAL([have_gl], [test x$have_gl_with_glfw = xyes || test x$have_headless_gl = xyes])
AS_IF([test x$have_gl_with_glfw = xyes || test x$have_headless_gl = xyes],
	  [AC_DEFINE([HAVE_GL], [1], [Do we have OpenGL])],
	  [AC_MSG_WARN([Compiling without OpenGL support])])

have_cuda=yes
AC_ARG_ENABLE([cuda], [AS_HELP_STRING([--disable-cuda], [Do not use Cuda, even if present])], [have_cuda=$enableval], [have_cuda=yes])
AC_CHECK_LIB([cuda], [cuInit], [], [have_cuda=no])
AC_CHECK_LIB([cudart], [cudaMalloc], [], [have_cuda=no])
AC_CHECK_LIB([curand], [main], [], [have_cuda=no])
AM_CONDITIONAL([have_cuda], [test x$have_cuda = xyes])
AS_IF([test x$have_cuda = xyes],
	  [AC_DEFINE([HAVE_CUDA], [1], [Do we have CUDA])],
	  [AC_MSG_WARN([Compiling without CUDA support])])

have_embree=yes
AC_CHECK_LIB([embree3], [main], [], [have_embree=no])
AM_CONDITIONAL([have_embree], [test x$have_embree = xyes])

have_optix=yes
AC_CHECK_HEADER([optix.h], [], [have_optix=no])
AM_CONDITIONAL([have_optix], [test x$have_optix = xyes])
AS_IF([test x$have_optix = xyes],
	  [AC_DEFINE([HAVE_OPTIX], [1], [Do we have OptiX])],
	  [AC_MSG_WARN([Compiling without OptiX support])])

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
				rt/cpu/Makefile
				rt/gl/Makefile
				rt/cuda/Makefile
				gi/Makefile
				scripts/Makefile
				render-data/Makefile
				])

AC_OUTPUT
