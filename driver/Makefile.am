bin_PROGRAMS = rtgi

#rt_CXXFLAGS = $(WAND_CFLAGS)

# todo: add headers
rtgi_SOURCES = main.cpp cmdline.cpp interaction.cpp
noinst_HEADERS = interaction.h cmdline.h

rtgi_LDADD = ../gi/libprimary-hit.a ../rt/bbvh-base/libbbvh-base.a ../libgi/libgi.a
rtgi_LDADD += $(WAND_LIBS)
