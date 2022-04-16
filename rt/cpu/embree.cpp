#include "embree.h"

embree_tracer::embree_tracer() {
    em_device = rtcNewDevice(0);

    //Error handling
    if (!em_device)
        std::cerr << "Cannot create embree device. Error code: " << rtcGetDeviceError(0) << "\n";
    rtcSetDeviceErrorFunction(em_device, [](void* userPtr, enum RTCError error, const char* str) {
	                                     	std::cerr << "Embree error: Code " << error << ": " << str <<"\n";
										 },
										 nullptr);

    em_scene = rtcNewScene(em_device);
    rtcSetSceneBuildQuality(em_scene, RTC_BUILD_QUALITY_HIGH);
}

embree_tracer::~embree_tracer() {
    rtcReleaseScene(em_scene);
    rtcReleaseDevice(em_device);
}

//This expects everything in the szene to be one merged triangle mesh
//Other geometry types (NURBS etc) are supported by embree,
//but we would need to set additional buffers here depending on the geometry type
void embree_tracer::build(::scene *scene) {
    RTCGeometry geom = rtcNewGeometry(em_device, RTC_GEOMETRY_TYPE_TRIANGLE);
    //No need to create a new buffer, we just tell embree how our buffers look like
	rtcSetSharedGeometryBuffer(geom,
							   RTC_BUFFER_TYPE_INDEX,
							   0,
							   RTC_FORMAT_UINT3,
							   &(scene->triangles[0]),
							   0,
							   4*sizeof(uint32_t),
							   scene->triangles.size());
	rtcSetSharedGeometryBuffer(geom,
							   RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
							   &(scene->vertices[0]),
							   0,
							   8*sizeof(float),
							   scene->vertices.size());

    rtcCommitGeometry(geom);
    rtcAttachGeometry(em_scene, geom);
    //The geometry is now managed by the scene, we can release it
    rtcReleaseGeometry(geom);
    rtcCommitScene(em_scene);
}

RTCRay ray_convert_to_embree(const ray &ray)
{
    //Just a 1-1 conversion into the struct embree expects
    RTCRay em_ray;
    em_ray.org_x = ray.o.x;
    em_ray.org_y = ray.o.y;
    em_ray.org_z = ray.o.z;
    em_ray.dir_x = ray.d.x;
    em_ray.dir_y = ray.d.y;
    em_ray.dir_z = ray.d.z;
    em_ray.tnear = ray.t_min;
    em_ray.tfar = ray.t_max;
    //These are just defaults, but need to be set if ray masking/motion blur is wanted
    em_ray.mask = -1;
    em_ray.flags = 0;
    em_ray.time = 0;
    return em_ray;
}

bool embree_tracer::any_hit(const ray &ray)
{
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);
    RTCRay em_ray = ray_convert_to_embree(ray);
    rtcOccluded1(em_scene, &context, &em_ray);

    //tfar is set to -inf if there is a hit
    return std::isinf(em_ray.tfar) && (em_ray.tfar) < 0;
}

triangle_intersection embree_tracer::closest_hit(const ray &ray)
{
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    RTCRayHit rayHit;
    rayHit.ray = ray_convert_to_embree(ray);
    rayHit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rtcIntersect1(em_scene, &context, &rayHit);

    //Again just a 1-1 conversion from the embree returned struct to ours
    triangle_intersection closest;
    if(rayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
        closest.t = rayHit.ray.tfar;
    else
        closest.t = FLT_MAX;
    //u and v correspond to beta and gamma, just different names
    closest.beta = rayHit.hit.u;
    closest.gamma = rayHit.hit.v;
    closest.ref = rayHit.hit.primID;

    return closest;
}
