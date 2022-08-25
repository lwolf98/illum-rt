#include "optix-tracer.h"
#include "platform.h"
#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <optix_stack_size.h>
#include "embedded-ptx-code.h"
#include <cuda-operators.h>

/* Short overall OptiX concept:
 * 
 * Modules: 
 * Optix compiles the ptx-code of device-code files that contain optix-specific kernels
 * into modules.
 * 
 * Programs:
 * Programs map to functions within a module and also describe them.
 *  
 * Pipeline:
 * The pipeline is a collection of all existing programs.
 * 
 * Shader binding table (SBT):
 * The shader binding is the configuration of a pipeline launch.
 * It tells OptiX which programs to invoke.
 * It is also used to map acceleration-structure-build-inputs to specific
 * programs (defined by an sbt-offset).
 * On top of that it provides program specific user-data which
 * can be attached to the sbt record and then be queried in the program.
 * 
 * For further information check the OptiX 7 Programming Guide.
 * 
 */


#define CURRENT_CUDA_CONTEXT 0

/* Those function names are looked up in the provided ptx code.
 * They are passed to the different programs and map the program to the function.
 * Important note: The function names follow a naming convention - dependent on the
 * program type the function has to have a specific prefix as shown below and
 * in the OptiX 7 documentation.
 */
constexpr const char *RAYGEN_ENTRY_FUNCTION_NAME = "__raygen__render_frame";
constexpr const char *MISS_ENTRY_FUNCTION_NAME = "__miss__radiance";
constexpr const char *CLOSESTHIT_ENTRY_FUNCTION_NAME = "__closesthit__radiance";
constexpr const char *ANYHIT_ALPHA_ENTRY_FUNCTION_NAME = "__anyhit__radiance_alpha";
constexpr const char *ANYHIT_ENTRY_FUNCTION_NAME = "__anyhit__radiance";
constexpr const char *LAUNCH_PARAMS_VARIABLE_NAME = "launch_params";


// Configuration for stack sizes within the pipeline.

/* How many acceleration structures (or levels of acceleration structures) do we have?
 * There is only a GAS in the current tracer.
 */
constexpr const unsigned int MAX_TRAVERSABLE_GRAPH_DEPTH = 1;

/* How many nested optixTrace calls do we have within our kernels?
 * 
 * Normal case: (MAX_TRACE_CALL_DEPTH - used when alpha_aware == false)
 * We only call optixTrace within the raygen program.
 * Note: The raygen program does not generate our rays even though the name suggests it.
 */
constexpr const unsigned int MAX_TRACE_CALL_DEPTH = 1;

/* Continuous callables are like direct callables (see below). The only difference is
 * that continuous callables can call optixTrace and thus have to be managed
 * differently by OptiX.  
 */
constexpr const unsigned int MAX_CONTINUOUS_CALLABLE_CALL_DEPTH = 0;

/* Direct callables are functions which the location of is stored in 
 * the shader binding table (the configuration of the optixLaunch).
 * Within the kernel code they are invoked via the sbt and an index.
 * Those functions provide flexibility and the ability to change the called function
 * without recompiling the program.
 */
constexpr const unsigned int MAX_DIRECT_CALLABLE_CALL_DEPTH = 0;


namespace wf::cuda {
    optix_tracer::optix_tracer(bool alpha_aware) : alpha_aware(alpha_aware),
                                                   cuda_context(CURRENT_CUDA_CONTEXT),
                                                   sbt{},
                                                   verbose(false),
                                                   optix_pipeline_link_options{},
                                                   optix_pipeline_compile_options{},
                                                   optix_context(nullptr),
                                                   raygen_records_buffer("raygen records buffer", 1),
                                                   miss_records_buffer("miss records buffer", 1),
                                                   hitgroup_records_buffer("hitgroup records buffer", 1),
                                                   device_launch_params("optix launch params", 1),
                                                   accel_struct_buffer("accell struct buffer", 0) {
        init_optix();
        create_context();
        create_module();

        OptixProgramGroupOptions program_group_options{};
        
        OptixProgramGroupDesc raygen_program_desc{};
        raygen_program_desc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
        raygen_program_desc.raygen.module = optix_module;
        raygen_program_desc.raygen.entryFunctionName = RAYGEN_ENTRY_FUNCTION_NAME;
        create_program(raygen_program, program_group_options, raygen_program_desc);

        OptixProgramGroupDesc miss_program_desc{};
        miss_program_desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
        miss_program_desc.miss.module = optix_module;
        miss_program_desc.miss.entryFunctionName = MISS_ENTRY_FUNCTION_NAME;
        create_program(miss_program, program_group_options, miss_program_desc);

        OptixProgramGroupDesc hitgroup_program_desc{};
        hitgroup_program_desc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        hitgroup_program_desc.hitgroup.moduleCH = optix_module;
        hitgroup_program_desc.hitgroup.moduleAH = optix_module;
        hitgroup_program_desc.hitgroup.entryFunctionNameCH = CLOSESTHIT_ENTRY_FUNCTION_NAME;
        hitgroup_program_desc.hitgroup.entryFunctionNameAH = alpha_aware ? ANYHIT_ALPHA_ENTRY_FUNCTION_NAME : ANYHIT_ENTRY_FUNCTION_NAME;
        create_program(hitgroup_program, program_group_options, hitgroup_program_desc);

        create_pipeline();
        build_sbt();
    }

    void optix_tracer::compute_hit(bool anyhit) {    
        if (anyhit)
            host_launch_params.ray_flags = OptixRayFlags::OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT; 
        else 
            host_launch_params.ray_flags = alpha_aware ? OptixRayFlags::OPTIX_RAY_FLAG_NONE : OptixRayFlags::OPTIX_RAY_FLAG_DISABLE_ANYHIT;
        
        host_launch_params.rays = rd->rays.device_memory;
        host_launch_params.triangle_intersections = rd->intersections.device_memory;
        device_launch_params.upload(1, &host_launch_params);
        
        CHECK_OPTIX_ERROR(optixLaunch(optix_pipeline,
                                      cuda_stream,
                                      static_cast<CUdeviceptr>(device_launch_params),
                                      device_launch_params.size_in_bytes(),
                                      &sbt,
                                      host_launch_params.frame_buffer_dimensions.x,
                                      host_launch_params.frame_buffer_dimensions.y,
                                      1), "");

        CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
        CHECK_CUDA_ERROR(cudaGetLastError(), ""); 
    }
    
    /* Builds and compacts the acceleration structure. 
     * The acceleration structure has to be provided when calling optixTrace in device code
     * and should thus remain within the launch parameters. 
     * This makes the handle to the acceleration structure accessible by every kernel which wants
     * to call optixTrace.
     */
    OptixTraversableHandle optix_tracer::build_acceleration_structure(scenedata *scene) {
        time_this_block(optix_build);

        using vertex_t = float4;
        using triangle_t = uint4;
        
        constexpr const size_t NUM_BUILD_INPUTS = 1;
        constexpr const size_t NUM_EMITTED_PROPERTIES = 1;

        CUdeviceptr vertices = static_cast<CUdeviceptr>(scene->vertex_pos);
        CUdeviceptr triangles = static_cast<CUdeviceptr>(scene->triangles);

        OptixBuildInput build_input{};
        OptixBuildInputTriangleArray &triangle_array = build_input.triangleArray;
        
        build_input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;

        triangle_array.vertexFormat = OptixVertexFormat::OPTIX_VERTEX_FORMAT_FLOAT3;
        triangle_array.vertexBuffers = &vertices;
        triangle_array.vertexStrideInBytes = sizeof(vertex_t);
        triangle_array.numVertices = scene->vertex_pos.size;

        triangle_array.indexFormat = OptixIndicesFormat::OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
        triangle_array.indexBuffer = triangles;
        triangle_array.indexStrideInBytes = sizeof(triangle_t);
        triangle_array.numIndexTriplets = scene->triangles.size;

        uint32_t triangle_array_flags[1] = { 0 };

        triangle_array.flags = triangle_array_flags;
        triangle_array.numSbtRecords = 1;
        triangle_array.sbtIndexOffsetBuffer = 0;
        triangle_array.sbtIndexOffsetSizeInBytes = 0;
        triangle_array.sbtIndexOffsetStrideInBytes = 0;

        OptixAccelBuildOptions optix_accel_build_options{};
        optix_accel_build_options.buildFlags = OPTIX_BUILD_FLAG_NONE | OPTIX_BUILD_FLAG_ALLOW_COMPACTION;
        optix_accel_build_options.motionOptions.numKeys = 1;
        optix_accel_build_options.operation = OPTIX_BUILD_OPERATION_BUILD;
        
        OptixAccelBufferSizes buffer_sizes;

        CHECK_OPTIX_ERROR(optixAccelComputeMemoryUsage(optix_context,
                                                       &optix_accel_build_options,
                                                       &build_input,
                                                       NUM_BUILD_INPUTS, 
                                                       &buffer_sizes), "");
        
     
        global_memory_buffer<uint64_t> compacted_size_buffer("OptiX compacted size accel build buffer", 1);

        OptixAccelEmitDesc emit_desc;
        emit_desc.type = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
        emit_desc.result = static_cast<CUdeviceptr>(compacted_size_buffer);

        // If we intend to build the bvh multiple times (e.g. each frame) the temporary buffers
        // should be stored as members to keep them for the next build.
        global_memory_buffer<char> temp_buffer("OptiX temporary accel build buffer", buffer_sizes.tempSizeInBytes);
        global_memory_buffer<char> output_buffer("OptiX output accel build buffer", buffer_sizes.outputSizeInBytes);
    
        CHECK_OPTIX_ERROR(optixAccelBuild(optix_context,
                                          cuda_stream,
                                          &optix_accel_build_options,
                                          &build_input,
                                          NUM_BUILD_INPUTS,
                                          static_cast<CUdeviceptr>(temp_buffer),
                                          temp_buffer.size_in_bytes(),
                                          static_cast<CUdeviceptr>(output_buffer),
                                          output_buffer.size_in_bytes(),
                                          &optix_accel_traversable_handle,
                                          &emit_desc,
                                          NUM_EMITTED_PROPERTIES), "");

        CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
        CHECK_CUDA_ERROR(cudaGetLastError(), "");

        compacted_size_buffer.download();
        
        accel_struct_buffer.resize(compacted_size_buffer.host_data[0]);
       
        CHECK_OPTIX_ERROR(optixAccelCompact(optix_context,
                                            cuda_stream,
                                            optix_accel_traversable_handle,
                                            static_cast<CUdeviceptr>(accel_struct_buffer),
                                            accel_struct_buffer.size_in_bytes(),
                                            &optix_accel_traversable_handle), "");
        
        CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
        CHECK_CUDA_ERROR(cudaGetLastError(), "");
        
        return optix_accel_traversable_handle;  
    }

    /* Builds the acceleration structure and sets host launch parameters.
     * The host-launch-parameters-structure contains data which gets copied to constant memory
     * when calling optixLaunch. Since those parameters are stored globally they
     * can be accessed by any device function.
     */
    void optix_tracer::build(scenedata *scenedata) {
        scene_data = scenedata;
        
        rd = new raydata(rc->resolution());

        host_launch_params.optix_traversable_handle = build_acceleration_structure(scenedata);
        host_launch_params.frame_buffer_dimensions.x = rc->resolution().x;
        host_launch_params.frame_buffer_dimensions.y = rc->resolution().y;

        host_launch_params.materials = scene_data->materials.device_memory;
        host_launch_params.tex_coords = scene_data->vertex_tc.device_memory;
        host_launch_params.triangles = scene_data->triangles.device_memory;
    }

    void optix_tracer::init_optix() {
        CHECK_OPTIX_ERROR(optixInit(), "");
    }

    void optix_tracer::create_context() {
        CHECK_CUDA_ERROR(cudaStreamCreate(&cuda_stream), "");

        const char *error_name;
        const char *error_string;
 
        CUresult status = cuCtxGetCurrent(&cuda_context);
        
        if (status != CUDA_SUCCESS) {
            std::cerr << "Error querying current CUDA context. Error code is " << status  << std::endl;
            
            if (cuGetErrorName(status, &error_name) == CUDA_SUCCESS)   
                std::cerr << "CUDA error: " << error_name << std::endl;
            else
                std::cerr << "CUDA error: Cannot retrieve error name" << std::endl;
            
            if (cuGetErrorString(status, &error_string) == CUDA_SUCCESS)
                std::cerr << "CUDA error: " << error_string << std::endl;
            else
                std::cerr << "CUDA error: Cannot retrieve error string" << std::endl;
            
            throw std::runtime_error("Cannot query current CUDA-Context");
        }
        
        OptixDeviceContextOptions optix_device_context_options{};
        CHECK_OPTIX_ERROR(optixDeviceContextCreate(cuda_context, &optix_device_context_options, &optix_context), "");
    }

    /* Creates the OptiX module from the generated ptx-code.
     * A module is a compiled version of the ptx-code / the device-kernels / the different programs.
     * In this specific case there is only one device-code-file / ptx-code-file which results in
     * only having one module but there could be more modules containing optix-specific device code.
     */
    void optix_tracer::create_module() {
        OptixModuleCompileOptions optix_module_compile_options{};
        optix_module_compile_options.maxRegisterCount = OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT;
        optix_module_compile_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
        optix_module_compile_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_NONE;
        
        optix_pipeline_compile_options.traversableGraphFlags = OptixTraversableGraphFlags::OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS;
        optix_pipeline_compile_options.usesMotionBlur = false;
        optix_pipeline_compile_options.numPayloadValues = 2;
        optix_pipeline_compile_options.numAttributeValues = 2;
        optix_pipeline_compile_options.exceptionFlags = OptixExceptionFlags::OPTIX_EXCEPTION_FLAG_NONE;
        optix_pipeline_compile_options.pipelineLaunchParamsVariableName = LAUNCH_PARAMS_VARIABLE_NAME;
        
        optix_pipeline_link_options.maxTraceDepth = 1;
        const std::string ptx_code(reinterpret_cast<const char*>(embedded_ptx_code));
        
        char log[2048];
        size_t size_of_log = sizeof(log);

        CHECK_OPTIX_ERROR(optixModuleCreateFromPTX(optix_context,
                                                   &optix_module_compile_options,
                                                   &optix_pipeline_compile_options,
                                                   ptx_code.c_str(),
                                                   ptx_code.size(),
                                                   log,
                                                   &size_of_log,
                                                   &optix_module), "");

        if (size_of_log > 1 && verbose)
            std::cout << log << std::endl;
    };

    void optix_tracer::create_program(OptixProgramGroup &program_group, OptixProgramGroupOptions &program_group_options, OptixProgramGroupDesc &program_group_descriptor) {       
        char log[2048];
        size_t size_of_log = sizeof(log);

        CHECK_OPTIX_ERROR(optixProgramGroupCreate(optix_context,
                                                  &program_group_descriptor,
                                                  1,
                                                  &program_group_options,
                                                  log,
                                                  &size_of_log,
                                                  &program_group), "");
        
        if (size_of_log > 1 && verbose)
            std::cout << log << std::endl;
    }

    /* Creates the OptiX Pipeline using the previously created program groups.
     * Also sets the call-stack-sizes for direct/continuous callables by calling utility functions
     * which calculate the accumulated upper stack size bounds for all programs.
     * 
     * If the kernel functions such as raygen, anyhit, closesthit etc. change one has to
     * adapt the constants MAX_TRACE_CALL_DEPTH, MAX_CONTINUOUS_CALLABLE_CALL_DEPTH, MAX_DIRECT_CALLABLE_CALL_DEPTH.
     * 
     * If there are multiple acceleration structures (e.g. TLAS) the constant MAX_TRAVERSABLE_GRAPH_DEPTH has to be adapted.
     */
    void optix_tracer::create_pipeline() {
        std::vector<OptixProgramGroup> program_groups { raygen_program, miss_program, hitgroup_program };
        
        char log [2048];
        size_t size_of_log = sizeof(log);

        CHECK_OPTIX_ERROR(optixPipelineCreate(optix_context,
                                              &optix_pipeline_compile_options,
                                              &optix_pipeline_link_options,
                                              program_groups.data(),
                                              program_groups.size(),
                                              log,
                                              &size_of_log,
                                              &optix_pipeline), "");

        if (size_of_log > 1 && verbose)
            std::cout << log << std::endl;

        OptixStackSizes accumulated_stack_sizes{};

        for (auto &program_group: program_groups)
            CHECK_OPTIX_ERROR(optixUtilAccumulateStackSizes(program_group, &accumulated_stack_sizes), "");
        
        unsigned int direct_callable_stack_size_from_traversal;
        unsigned int direct_callable_stack_size_from_state;
        unsigned int continuous_callable_stack_size;
        CHECK_OPTIX_ERROR(optixUtilComputeStackSizes(&accumulated_stack_sizes,
                                                     MAX_TRACE_CALL_DEPTH,
                                                     MAX_CONTINUOUS_CALLABLE_CALL_DEPTH,
                                                     MAX_DIRECT_CALLABLE_CALL_DEPTH,
                                                     &direct_callable_stack_size_from_traversal,
                                                     &direct_callable_stack_size_from_state,
                                                     &continuous_callable_stack_size), 
                                                     "");

        CHECK_OPTIX_ERROR(optixPipelineSetStackSize(optix_pipeline,
                                                    direct_callable_stack_size_from_traversal,
                                                    direct_callable_stack_size_from_state,
                                                    continuous_callable_stack_size,
                                                    MAX_TRAVERSABLE_GRAPH_DEPTH), "");
    }

    /* Builds the shader binding table (sbt) which has to be provided when launching 
     * the OptiX-Pipeline.
     * For further details check "Short overall OptiX concept" at the beginning of this file.
     */
    void optix_tracer::build_sbt() {     
        raygen_record tmp_raygen_record;
        CHECK_OPTIX_ERROR(optixSbtRecordPackHeader(raygen_program, &tmp_raygen_record), "");
        raygen_records_buffer.upload(1, &tmp_raygen_record);
        sbt.raygenRecord = static_cast<CUdeviceptr>(raygen_records_buffer);
        
        miss_record tmp_miss_record;
        CHECK_OPTIX_ERROR(optixSbtRecordPackHeader(miss_program, &tmp_miss_record), "");

        miss_records_buffer.upload(1, &tmp_miss_record);
        sbt.missRecordBase = static_cast<CUdeviceptr>(miss_records_buffer);
        sbt.missRecordStrideInBytes = sizeof(miss_record);
        sbt.missRecordCount = 1;
    
        hitgroup_record tmp_hitgroup_record;
        CHECK_OPTIX_ERROR(optixSbtRecordPackHeader(hitgroup_program, &tmp_hitgroup_record), "");
        hitgroup_records_buffer.upload(1, &tmp_hitgroup_record);
        sbt.hitgroupRecordBase = static_cast<CUdeviceptr>(hitgroup_records_buffer);
        sbt.hitgroupRecordStrideInBytes = sizeof(hitgroup_record);
        sbt.hitgroupRecordCount = 1;
    }  
}
