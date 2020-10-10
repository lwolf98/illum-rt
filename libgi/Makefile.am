noinst_LIBRARIES = libgi.a

libgi_a_CXXFLAGS = $(WAND_CFLAGS)
#libgi_a_LIBADD = $(WAND_LIBS)

libgi_a_SOURCES = 	camera.cpp \
					framebuffer.cpp \
					random.cpp \
					rt.cpp \
					scene.cpp \
					timer.cpp

#ifndef RTGI_A02
libgi_a_SOURCES +=  material.cpp
#endif

#ifndef RTGI_AXX
libgi_a_SOURCES +=  discrete_distributions.cpp \
					material.cpp
#endif

noinst_HEADERS = 	algorithm.h \
					camera.h \
					color.h \
					context.h \
					framebuffer.h \
					intersect.h \
					material.h \
					random.h \
					rt.h \
					scene.h \
					timer.h \
					util.h


#ifndef RTGI_AXX
noinst_HEADERS +=	discrete_distributions.h \
					sampling.h
#endif
