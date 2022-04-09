noinst_LIBRARIES = libcudaraytracing.a
libcudaraytracing_a_SOURCES = base.cu \
						      cuda-helpers.cpp \
							  kernels.cu \
							  rni.cu \
							  tracers.cu
noinst_HEADERS = base.h \
			     cuda-helpers.h \
				 kernels.h \
				 rni.h \
				 tracers.h

GENCODE_FLAGS  = -gencode arch=compute_86,code=sm_86
NVCC_FLAGS = -Xptxas -v -Xcudafe --diag_suppress=esa_on_defaulted_function_ignored --resource-usage --generate-line-info --use_fast_math

.cu.o:
	$(top_srcdir)/nvcc-wrapper -std=c++17 $(NVCC_FLAGS) $(DEFAULT_INCLUDES) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $< -v $(GENCODE_FLAGS)
