#pragma once

#include <optix.h>

#include <cuda_runtime.h>
#include <cuda_runtime_api.h>

#include <vector>

#include "optix-launch-params.h"
#include "optix-helper.h"
#include "optix-records.h"

#include "base.h"


namespace wf::cuda {
    class optix_tracer: public wf::cuda::batch_rt {
        public:
            optix_tracer();

            void build(scenedata *scene) override;
            void compute_hit(bool anyhit) override;

        protected:
            void create_program(OptixProgramGroup &program_group, OptixProgramGroupOptions &program_group_options, OptixProgramGroupDesc &program_group_descriptor);
        
            void init_optix();
            void create_context();
            void create_module();
            void create_pipeline();
            void build_sbt();

            OptixTraversableHandle build_acceleration_structure(wf::cuda::scenedata *scene);

            scenedata *scene_data;

            CUstream cuda_stream;
            CUcontext cuda_context;
    
            OptixDeviceContext optix_context;

            OptixPipeline optix_pipeline;
            OptixPipelineCompileOptions optix_pipeline_compile_options;
            OptixPipelineLinkOptions optix_pipeline_link_options;

            OptixModule optix_module;

            OptixTraversableHandle optix_accel_traversable_handle;
            global_memory_buffer<char> accel_struct_buffer;
            
            OptixProgramGroup raygen_program;
            OptixProgramGroup miss_program;
            OptixProgramGroup hitgroup_program;

            OptixShaderBindingTable sbt;

            global_memory_buffer<hitgroup_record> hitgroup_records_buffer;
            global_memory_buffer<miss_record> miss_records_buffer;
            global_memory_buffer<raygen_record> raygen_records_buffer;
            
            // The launch params get copied into constant memory once
            // optixLaunch gets called and are accessible within the kernels
            optix_launch_params host_launch_params;
            global_memory_buffer<optix_launch_params> device_launch_params;

            bool verbose;
    };
}