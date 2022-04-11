SUFFIXES = .cu
noinst_LIBRARIES = libcudaraytracing.a
libcudaraytracing_a_CUDA_SOURCES = base.cu \
							  kernels.cu \
							  rni.cu \
							  tracers.cu

libcudaraytracing_a_SOURCES = cuda-helpers.cpp $(libcudaraytracing_a_CUDA_SOURCES)

noinst_HEADERS = base.h \
			     cuda-helpers.h \
				 cuda-operators.h \
				 kernels.h \
				 rni.h \
				 tracers.h

GENCODE_FLAGS  = -gencode arch=compute_86,code=sm_86
NVCC_FLAGS = -Xptxas -v -Xcudafe --diag_suppress=esa_on_defaulted_function_ignored --resource-usage --generate-line-info --use_fast_math

.cu.o:
	$(top_srcdir)/nvcc-wrapper -std=c++17 $(NVCC_FLAGS) $(DEFAULT_INCLUDES) $(CPPFLAGS) $(CXXFLAGS) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Po -c -o $@ $< -v $(GENCODE_FLAGS)

cuda_deps = $(libcudaraytracing_a_CUDA_SOURCES:%.cu=$(DEPDIR)/%.Po)
sinclude $(cuda_deps)
