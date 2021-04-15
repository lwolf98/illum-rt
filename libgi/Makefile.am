noinst_LIBRARIES = libgi.a

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

#ifndef RTGI_A02
libgi_a_SOURCES +=  material.cpp
#endif

#ifndef RTGI_A05
libgi_a_SOURCES +=  discrete_distributions.cpp
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


#ifndef RTGI_A05
noinst_HEADERS +=	discrete_distributions.h
#endif
#ifndef RTGI_A04
noinst_HEADERS +=	sampling.h
#endif
