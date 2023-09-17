AM_CPPFLAGS = -I$(top_srcdir)

#ifndef RTGI_SKIP_WF
if have_cmera
BUILT_SOURCES = direct-steps.h primary-steps.h

$(srcdir)/primary-steps.h: primary-steps.lisp
	cm c++ -D top_srcdir=$(top_srcdir) $< -o $@
$(srcdir)/direct-steps.h: direct-steps.lisp primary-steps.h
	cm c++ -D top_srcdir=$(top_srcdir) $< -o $@
endif
#endif

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

#ifndef RTGI_SKIP_WF
noinst_HEADERS +=  primary-steps.h direct-steps.h
EXTRA_DIST  = primary-steps.lisp direct-steps.lisp
EXTRA_DIST += primary-steps.h    direct-steps.h
#endif
