AM_CPPFLAGS = -I$(top_srcdir)

noinst_LIBRARIES = libprimary-hit.a
libprimary_hit_a_SOURCES = primary-hit.cpp
noinst_HEADERS = primary-hit.h

#ifndef RTGI_SKIP_DIRECT_ILLUM
noinst_LIBRARIES += libdirect.a 
libdirect_a_SOURCES = direct.cpp
noinst_HEADERS += direct.h
#endif

#ifndef RTGI_SKIP_SIMPLE_PT
noinst_LIBRARIES += libpt.a
libpt_a_SOURCES = pt.cpp
noinst_HEADERS += pt.h
#endif

