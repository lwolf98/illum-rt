AM_CPPFLAGS = -I$(top_srcdir)
SUBDIRS = libgi gi rt debug driver scripts render-data
EXTRA_DIST = README.md nvcc-wrapper

all-local: rtgi

rtgi:
	-ln -s driver/rtgi rtgi
