noinst_LIBRARIES = libglrt.a

libglrt_a_SOURCES = seq.cpp base.cpp rni.cpp ray-setup-glsl.cpp
noinst_HEADERS = seq.h

%-glsl.cpp: %.glsl
	echo '#include "shader.h"' > $@
	echo 'namespace wf::gl {' >> $@
	echo '	'compute_shader $*'_shader("'$*'", R"(' | tr - _ >> $@
	m4 $< >> $@
	echo '	)");' >> $@
	echo '}' >> $@
