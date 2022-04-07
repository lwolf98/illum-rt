noinst_LIBRARIES = libglrt.a

glsl_GENSOURCES = ray-setup-glsl.cpp seq-closest-glsl.cpp seq-any-glsl.cpp bvh-closest-glsl.cpp
libglrt_a_SOURCES = debug.cpp find-hit.cpp base.cpp rni.cpp opengl.cpp $(glsl_GENSOURCES)
noinst_HEADERS = find-hit.h opengl.h shader.h base.h rni.h
BUILT_SOURCES = glsl.deps $(glsl_GENSOURCES)
CLEANFILES = glsl.deps $(glsl_GENSOURCES)
glsl_SRC = $(glsl_GENSOURCES:%-glsl.cpp=%.glsl)
EXTRA_DIST = $(glsl_SRC)

%-glsl.cpp: %.glsl
	echo '#include "shader.h"' > $@
	echo 'namespace wf::gl {' >> $@
	echo '	'compute_shader $*'_shader("'$*'", R"(' | tr - _ >> $@
	m4 $< >> $@
	echo '	)");' >> $@
	echo '}' >> $@

glsl.deps: $(glsl_GENSOURCES:%-glsl.cpp=%.glsl) Makefile
	bash depcomp.glsl $^ > glsl.deps

sinclude glsl.deps
