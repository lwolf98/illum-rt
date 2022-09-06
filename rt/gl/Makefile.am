noinst_LIBRARIES = libglrt.a

AM_CPPFLAGS = -I$(top_srcdir)

glsl_GENSOURCES = ray-setup-glsl.cpp \
				  seq-closest-glsl.cpp \
				  seq-any-glsl.cpp \
				  bvh-closest-glsl.cpp \
				  clear-framebuffer-glsl.cpp \
				  add-hitpoint-albedo-glsl.cpp \
				  add-hitpoint-albedo-hackytex-glsl.cpp \
				  add-hitpoint-albedo-plain-glsl.cpp \
				  copy-to-preview-glsl.cpp

libglrt_a_SOURCES = debug.cpp \
					find-hit.cpp \
					base.cpp \
					rni.cpp \
					opengl.cpp \
					platform.cpp \
					preprocessing.cpp \
					$(glsl_GENSOURCES)

noinst_HEADERS = find-hit.h opengl.h base.h rni.h platform.h preprocessing.h
BUILT_SOURCES = glsl.deps $(glsl_GENSOURCES)
CLEANFILES = glsl.deps $(glsl_GENSOURCES)
glsl_SRC = $(glsl_GENSOURCES:%-glsl.cpp=%.glsl)
EXTRA_DIST = $(glsl_SRC)

%-glsl.cpp: %.glsl
	echo '#include "libgi/gl/shader.h"' > $@
	echo 'namespace wf::gl {' >> $@
	echo '	'compute_shader $$(basename $*)'_shader("'$*'", R"(' | tr - _ >> $@
	m4 -I$(srcdir) $< >> $@
	echo '	)");' >> $@
	echo '}' >> $@

glsl.deps: $(glsl_GENSOURCES:%-glsl.cpp=%.glsl) Makefile
	bash $(srcdir)/depcomp.glsl $^ > glsl.deps

sinclude glsl.deps
