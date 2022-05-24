SUFFIXES = .cu
noinst_LIBRARIES = libcudaraytracing.a
libcudaraytracing_a_CUDA_SOURCES = kernels.cu \
								   preprocessing-kernels.cu \
								   trace-kernels.cu \
								   tracers.cu

libcudaraytracing_a_SOURCES = base.cpp cuda-helpers.cpp platform.cpp preprocessing.cpp rni.cpp $(libcudaraytracing_a_CUDA_SOURCES)

noinst_HEADERS = cuda-helpers.h \
				 cuda-operators.h \
				 kernels.h \
				 platform.h \
				 preprocessing.h \
				 rni.h \
				 tracers.h
				
if have_optix
libcudaraytracing_a_CUDA_SOURCES += optix-tracer.cu
libcudaraytracing_a_SOURCES += embedded-ptx-code.cpp optix-helper.cpp
noinst_HEADERS += optix-helper.h optix-records.h optix-launch-params.h embedded-ptx-code.h
BUILT_SOURCES = embedded-ptx-code.cpp embedded-ptx-code.ptx
CLEANFILES = embedded-ptx-code.cpp embedded-ptx-code.ptx
endif


AM_CPPFLAGS = -I$(top_srcdir)
GENCODE_FLAGS  = -gencode arch=compute_86,code=sm_86
NVCC_FLAGS = -Xptxas -v -Xcudafe --diag_suppress=esa_on_defaulted_function_ignored --resource-usage --generate-line-info --use_fast_math

embedded-ptx-code.ptx: optix-kernels.cu
	$(top_srcdir)/nvcc-wrapper -std=c++17 --ptx -rdc=true -I. -I../../ $(NVCC_FLAGS) $(AM_CPPFLAGS) $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

embedded-ptx-code.cpp: embedded-ptx-code.ptx
	(echo "#include \"embedded-ptx-code.h\""; bin2c -c --padd 0 --type char --name embedded_ptx_code $<) > $@

.cu.o:
	$(top_srcdir)/nvcc-wrapper -std=c++17 $(NVCC_FLAGS) $(DEFAULT_INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(CXXFLAGS) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Po -c -o $@ $< -v $(GENCODE_FLAGS)

cuda_deps = $(libcudaraytracing_a_CUDA_SOURCES:%.cu=$(DEPDIR)/%.Po)
sinclude $(cuda_deps)
