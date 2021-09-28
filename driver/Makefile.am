bin_PROGRAMS = rtgi

rtgi_SOURCES = main.cpp cmdline.cpp interaction.cpp
noinst_HEADERS = interaction.h cmdline.h

rtgi_LDADD  = ../gi/libprimary-hit.a ../rt/seq/libseq-is.a
#ifndef RTGI_SKIP_DIRECT_ILLUM
rtgi_LDADD += ../gi/libdirect.a
#endif
#ifndef RTGI_A06
rtgi_LDADD += ../gi/libpt.a
#endif
#ifndef RTGI_SKIP_BVH
rtgi_LDADD += ../rt/bbvh-base/libbbvh-base.a 
#endif
rtgi_LDADD += ../libgi/libgi.a
rtgi_LDADD += $(WAND_LIBS)
