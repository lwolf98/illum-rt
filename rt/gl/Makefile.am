noinst_LIBRARIES = libglrt.a

AM_CPPFLAGS = -I$(top_srcdir)

glsl_GENSOURCES = ray-setup-glsl.cpp \
				  seq-closest-glsl.cpp \
				  seq-any-glsl.cpp \
				  bvh-closest-glsl.cpp \
				  bvh-any-glsl.cpp \
				  clear-framebuffer-glsl.cpp \
				  add-hitpoint-albedo-glsl.cpp \
				  sample-light-glsl.cpp \
				  sample-uniform-glsl.cpp \
				  integrate-dir-sample-glsl.cpp \
				  integrate-light-sample-glsl.cpp \
				  copy-to-preview-glsl.cpp

libglrt_a_SOURCES = debug.cpp \
					find-hit.cpp \
					base.cpp \
					bounce.cpp \
					rni.cpp \
					opengl.cpp \
					platform.cpp \
					preprocessing.cpp \
					$(glsl_GENSOURCES)

noinst_HEADERS = base.h bounce.h find-hit.h opengl.h  platform.h preprocessing.h rni.h
BUILT_SOURCES = glsl.deps $(glsl_GENSOURCES)
CLEANFILES = glsl.deps $(glsl_GENSOURCES)
glsl_SRC = $(glsl_GENSOURCES:%-glsl.cpp=%.glsl)
EXTRA_DIST = $(glsl_SRC) preamble.glsl random.glsl

%-glsl.cpp: %.glsl
	echo '#include "libgi/gl/shader.h"' > $@
	echo 'namespace wf::gl {' >> $@
	echo '	'::gl::compute_shader $$(basename $*)'_shader("'$*'", R"(' | tr - _ >> $@
	m4 -I$(srcdir) @have_tex@ $< >> $@
	echo '	)");' >> $@
	echo '}' >> $@

glsl.deps: $(glsl_GENSOURCES:%-glsl.cpp=%.glsl) Makefile
	bash $(srcdir)/depcomp.glsl $^ > glsl.deps

sinclude glsl.deps
