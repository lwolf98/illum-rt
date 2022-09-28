noinst_LIBRARIES = libgi.a

AM_CPPFLAGS = -I$(top_srcdir)
libgi_a_CXXFLAGS = $(WAND_CFLAGS)
#libgi_a_LIBADD = $(WAND_LIBS)

libgi_a_SOURCES = 	algorithm.cpp \
					camera.cpp \
					context.cpp \
					framebuffer.cpp \
					random.cpp \
					rt.cpp \
					scene.cpp \
					timer.cpp


#ifndef RTGI_SKIP_BRDF
libgi_a_SOURCES +=  material.cpp
#endif

#ifndef RTGI_SKIP_LIGHT_SOURCE_SAMPLING
libgi_a_SOURCES +=  discrete_distributions.cpp
#endif

#ifndef RTGI_SKIP_WF
libgi_a_SOURCES += wavefront-rt.cpp
#endif

noinst_HEADERS = 	algorithm.h \
					camera.h \
					color.h \
					context.h \
					global-context.h \
					framebuffer.h \
					intersect.h \
					material.h \
					random.h \
					rt.h \
					scene.h \
					timer.h \
					util.h


#ifndef RTGI_SKIP_LIGHT_SOURCE_SAMPLING
noinst_HEADERS +=	discrete_distributions.h
#endif
#ifndef RTGI_SKIP_DIRECT_ILLUM
noinst_HEADERS +=	sampling.h
#endif
#ifndef RTGI_SKIP_WF
noinst_HEADERS +=   wavefront-rt.h
#endif

if have_gl
SUBDIRS = gl
endif
