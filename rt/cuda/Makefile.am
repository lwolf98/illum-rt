SUFFIXES = .cu
noinst_LIBRARIES = libcudaraytracing.a
libcudaraytracing_a_CUDA_SOURCES = kernels.cu \
								   trace-kernels.cu \
								   tracers.cu

libcudaraytracing_a_SOURCES = base.cpp cuda-helpers.cpp rni.cpp $(libcudaraytracing_a_CUDA_SOURCES)

noinst_HEADERS = cuda-helpers.h \
				 cuda-operators.h \
				 kernels.h \
				 platform.h \
				 rni.h \
				 tracers.h

AM_CPPFLAGS = -I$(top_srcdir)
GENCODE_FLAGS  = -gencode arch=compute_86,code=sm_86
NVCC_FLAGS = -Xptxas -v -Xcudafe --diag_suppress=esa_on_defaulted_function_ignored --resource-usage --generate-line-info --use_fast_math

.cu.o:
	$(top_srcdir)/nvcc-wrapper -std=c++17 $(NVCC_FLAGS) $(DEFAULT_INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(CXXFLAGS) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Po -c -o $@ $< -v $(GENCODE_FLAGS)

cuda_deps = $(libcudaraytracing_a_CUDA_SOURCES:%.cu=$(DEPDIR)/%.Po)
sinclude $(cuda_deps)
