noinst_LIBRARIES = libglrt.a

glsl_GENSOURCES = ray-setup-glsl.cpp seq-closest-glsl.cpp seq-any-glsl.cpp
libglrt_a_SOURCES = seq.cpp base.cpp rni.cpp $(glsl_GENSOURCES)
noinst_HEADERS = seq.h
BUILT_SOURCES = glsl.deps $(glsl_GENSOURCES)
CLEANFILES = glsl.deps $(glsl_GENSOURCES)

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
