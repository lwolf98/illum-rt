noinst_LIBRARIES = libcputracing.a

AM_CPPFLAGS = -I$(top_srcdir)

libcputracing_a_SOURCES = seq.cpp
noinst_HEADERS = seq.h

#ifndef RTGI_SKIP_BVH
libcputracing_a_SOURCES += bvh.cpp bvh2.cpp
noinst_HEADERS += bvh.h
#ifndef RTGI_SIMPLER_BBVH
noinst_HEADERS += bvh-ctor.h
#endif

if have_embree
libcputracing_a_SOURCES += embree.cpp
noinst_HEADERS += embree.h
endif
#endif

#ifndef RTGI_SKIP_WF
libcputracing_a_SOURCES += bounce.cpp denoise.cpp preprocessing.cpp wavefront.cpp
noinst_HEADERS += bounce.h denoise.h platform.h preprocessing.h wavefront.h
#endif
