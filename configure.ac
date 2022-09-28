AC_INIT([rtgi], [2022], [kai selgrad at oth regensburg de])

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

## 
## CHECK FOR GL
## 

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
have_tex=
AS_IF([test x$have_gl_with_glfw = xyes || test x$have_headless_gl = xyes],
	  [
	  AC_MSG_CHECKING([for GPU dynamically non-uniform access capabilities])
	  LSPCI="$(lspci  -v -s  $(lspci | grep ' VGA ' | cut -d" " -f 1))"
	  	if echo "$LSPCI" | grep NVIDIA >/dev/null ; then
			AC_MSG_RESULT([found NVIDIA GPU, using regular textures])
			AC_DEFINE([HAVE_DYN_NUF_TEX], [1], [We do have dynamically non-uniform texture access capabilities])
			have_tex="-D HAVE_TEX"
		else if echo "$LSPCI" | grep AMD >/dev/null ; then
			AC_MSG_RESULT([found AMD GPU, using a custom workaround])
			AC_DEFINE([HAVE_DYN_NUF_TEX_HACK], [2], [We do not have dynamically non-uniform texture access capabilities, but hack around it])
			have_tex="-D TEX_HACK"
		else
			AC_MSG_RESULT([found something not tested yet, or even unsupported, disabling surface textures])
		fi fi])
AC_SUBST([have_tex])

#ifndef RTGI_SKIP_WF
## 
## CHECK FOR CUDA and associated libs
## 

have_cuda=yes
AC_ARG_ENABLE([cuda], [AS_HELP_STRING([--disable-cuda], [Do not use Cuda, even if present])], [have_cuda=$enableval], [have_cuda=yes])
AC_CHECK_LIB([cuda], [cuInit], [], [have_cuda=no])
AC_CHECK_LIB([cudart], [cudaMalloc], [], [have_cuda=no])
AC_CHECK_LIB([curand], [main], [], [have_cuda=no])
AM_CONDITIONAL([have_cuda], [test x$have_cuda = xyes])
AS_IF([test x$have_cuda = xyes],
	  [AC_DEFINE([HAVE_CUDA], [1], [Do we have CUDA])],
	  [AC_MSG_WARN([Compiling without CUDA support])])

AC_ARG_WITH(compute-capability,
            [AS_HELP_STRING([--with-compute-capability], 
                [Which compute capability sould be used to compile the cuda files. We default to 86 currently, but lower versions might do for the non RTX version.])],
            compute_cap="$withval", compute_cap="86")
AC_SUBST([compute_cap])

have_optix=yes
AC_CHECK_HEADER([optix.h], [], [have_optix=no])
AM_CONDITIONAL([have_optix], [test x$have_optix = xyes])
AS_IF([test x$have_optix = xyes],
	  [AC_DEFINE([HAVE_OPTIX], [1], [Do we have OptiX])],
	  [AC_MSG_WARN([Compiling without OptiX support])])
#endif

#ifndef RTGI_SKIP_BVH
have_embree=yes
AC_CHECK_LIB([embree3], [main], [], [have_embree=no])
AM_CONDITIONAL([have_embree], [test x$have_embree = xyes])
AS_IF([test x$have_embree = xyes], [], [AC_MSG_WARN([Compiling without Embree support: CPU tracing will be slow])])
#endif

## 
## More libs and tools
## 

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


AC_CHECK_PROG([CM_CHECK], [cm], [yes], [no])
AM_CONDITIONAL([have_cmera], [test "x$CM_CHECK" == "xyes"])

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
				 libgi/gl/Makefile
				 rt/Makefile
				 rt/cpu/Makefile
				 gi/Makefile
				 scripts/Makefile
				 render-data/Makefile
				])
#ifndef RTGI_SKIP_WF
AC_CONFIG_FILES([rt/gl/Makefile
				 rt/cuda/Makefile
                ])
#endif
AC_OUTPUT
