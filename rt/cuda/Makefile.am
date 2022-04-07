noinst_LIBRARIES = libcudaraytracing.a
libcudaraytracing_a_SOURCES = base.cu \
						      cuda-helpers.cpp \
							  rni.cu \
							  tracers/kernels.cu \
							  tracers/simple.cu \
							  tracers/if-if.cu \
							  tracers/while-while.cu \
							  tracers/persistent-if-if.cu \
							  tracers/persistent-while-while.cu \
							  tracers/speculative-while-while.cu \
							  tracers/persistent-speculative-while-while.cu \
							  tracers/dynamic-while-while.cu
noinst_HEADERS = base.h \
			     cuda-helpers.h \
				 rni.h \
				 tracers/kernels.h \
				 tracers/simple.h \
				 tracers/if-if.h \
				 tracers/while-while.h \
				 tracers/persistent-if-if.h \
				 tracers/persistent-while-while.h \
				 tracers/speculative-while-while.h \
				 tracers/persistent-speculative-while-while.h \
				 tracers/dynamic-while-while.h

GENCODE_FLAGS  = -gencode arch=compute_86,code=sm_86
NVCC_FLAGS = -Xptxas -v -Xcudafe --diag_suppress=esa_on_defaulted_function_ignored --resource-usage --generate-line-info --use_fast_math

.cu.o:
	$(top_srcdir)/nvcc-wrapper -std=c++17 $(NVCC_FLAGS) $(DEFAULT_INCLUDES) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $< -v $(GENCODE_FLAGS)