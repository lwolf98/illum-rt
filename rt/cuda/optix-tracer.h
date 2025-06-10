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
            optix_tracer(bool alpha_aware);

            void build(scenedata *scene) override;
			void update_res(glm::ivec2 new_res) override;
            void compute_hit(bool anyhit) override;
        private:
            OptixTraversableHandle build_gas(wf::cuda::scenedata *scene, std::vector<OptixBuildInput> &build_inputs, OptixAccelBuildOptions &build_options, wf::cuda::global_memory_buffer<char> &accel_struct_buffer);

        protected:
            void create_program(OptixProgramGroup &program_group, OptixProgramGroupOptions &program_group_options, OptixProgramGroupDesc &program_group_descriptor);
        
            void init_optix();
            void create_context();
            void create_module();
            void create_pipeline();
            void build_sbt();

			uint32_t id_count = 0;

            /* If set to true the tracer will check wether the alpha value of the texture-coordinate which was hit (if there is any)
             * is above a certain threshold. If it's not the ray will be recast starting at the hit-coordinate. 
             */
            bool alpha_aware;

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
			global_memory_buffer<OptixInstance> optix_ias_instances;
            global_memory_buffer<char> accel_struct_buffer_tris;
            global_memory_buffer<char> accel_struct_buffer_patches;
            global_memory_buffer<char> accel_struct_buffer_ias;
            
            OptixProgramGroup raygen_program;
            OptixProgramGroup miss_program;
            OptixProgramGroup hitgroup_program_tris;
            OptixProgramGroup hitgroup_program_patches;

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