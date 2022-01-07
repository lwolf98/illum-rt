bin_PROGRAMS = rtgi

rtgi_SOURCES = main.cpp cmdline.cpp interaction.cpp
noinst_HEADERS = interaction.h cmdline.h

rtgi_LDADD  = ../gi/libprimary-hit.a ../rt/seq/libseq-is.a
#ifndef RTGI_SKIP_DIRECT_ILLUM
rtgi_LDADD += ../gi/libdirect.a
#endif
#ifndef RTGI_SKIP_SIMPLE_PT
rtgi_LDADD += ../gi/libpt.a
#endif
#ifndef RTGI_SKIP_BVH
rtgi_LDADD += ../rt/bbvh-base/libbbvh-base.a 
#endif
if have_gl
rtgi_LDADD += ../rt/gl/libglrt.a 
endif
if have_embree
rtgi_LDADD += ../rt/embree/libembree.a
endif
rtgi_LDADD += ../libgi/libgi.a
rtgi_LDADD += $(WAND_LIBS)
