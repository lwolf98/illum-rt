noinst_LIBRARIES = libcputracing.a

AM_CPPFLAGS = -I$(top_srcdir)

libcputracing_a_SOURCES = seq.cpp wavefront.cpp
noinst_HEADERS = seq.h wavefront.h platform.h

#ifndef RTGI_SKIP_BVH
libcputracing_a_SOURCES += bvh.cpp bvh2.cpp
noinst_HEADERS += bvh.h bvh-ctor.h

if have_embree
libcputracing_a_SOURCES += embree.cpp
noinst_HEADERS += embree.h
endif
#endif
