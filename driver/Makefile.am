bin_PROGRAMS = rtgi

AM_CPPFLAGS = -I$(top_srcdir)
rtgi_SOURCES = main.cpp cmdline.cpp interaction.cpp
noinst_HEADERS = interaction.h cmdline.h

if have_gl
rtgi_SOURCES += preview.cpp
noinst_HEADERS += preview.h
endif

rtgi_LDADD  = ../gi/libprimary-hit.a ../rt/cpu/libcputracing.a
#ifndef RTGI_SKIP_DIRECT_ILLUM
rtgi_LDADD += ../gi/libdirect.a
#endif
#ifndef RTGI_SKIP_SIMPLE_PT
rtgi_LDADD += ../gi/libpt.a
#endif

if have_gl
#ifndef RTGI_SKIP_WF
rtgi_LDADD += ../rt/gl/libglrt.a 
#endif
rtgi_LDADD += ../libgi/gl/libgl.a
endif
rtgi_LDADD += ../libgi/libgi.a
rtgi_LDADD += $(WAND_LIBS)

#ifndef RTGI_SKIP_WF
if have_cuda
rtgi_LDADD += ../rt/cuda/libcudaraytracing.a
endif
#endif

rtgi_LDADD += ../debug/libdebug.a