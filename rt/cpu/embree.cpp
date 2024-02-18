#include "embree.h"

#include "config.h"

#ifdef HAVE_LIBEMBREE3
#ifndef RTGI_SKIP_BVH
#ifndef RTGI_SIMPLER_BBVH

template <bool alpha_aware>
embree_tracer<alpha_aware>::embree_tracer() {
	em_device = rtcNewDevice(0);

	//Error handling
	if (!em_device)
		std::cerr << "Cannot create embree device. Error code: " << rtcGetDeviceError(0) << "\n";
	rtcSetDeviceErrorFunction(em_device,
							  [](void *user_ptr, enum RTCError error, const char *str) {
								  std::cerr << "Embree error: Code " << error << ": " << str <<"\n";
							  },
							  nullptr);

	em_scene = rtcNewScene(em_device);
	rtcSetSceneBuildQuality(em_scene, RTC_BUILD_QUALITY_HIGH);
}

template <bool alpha_aware>
embree_tracer<alpha_aware>::~embree_tracer() {
	rtcReleaseScene(em_scene);
	rtcReleaseDevice(em_device);
}

//This expects everything in the scene to be one merged triangle mesh
//Other geometry types (NURBS etc) are supported by embree,
//but we would need to set additional buffers here depending on the geometry type
template <bool alpha_aware>
void embree_tracer<alpha_aware>::build(::scene *scene) {
	this->scene = scene;
	RTCGeometry geom = rtcNewGeometry(em_device, RTC_GEOMETRY_TYPE_TRIANGLE);
	//No need to create a new buffer, we just tell embree how our buffers look like
	rtcSetSharedGeometryBuffer(geom,
							   RTC_BUFFER_TYPE_INDEX,
							   0,
							   RTC_FORMAT_UINT3,
							   &(scene->triangles[0]),
							   0,
							   sizeof(triangle),
							   scene->triangles.size());
	rtcSetSharedGeometryBuffer(geom,
							   RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
							   &(scene->vertices[0]),
							   0,
							   sizeof(vertex),
							   scene->vertices.size());

	rtcCommitGeometry(geom);
	rtcAttachGeometry(em_scene, geom);
	//The geometry is now managed by the scene, we can release it
	rtcReleaseGeometry(geom);
	rtcCommitScene(em_scene);
}

RTCRay ray_convert_to_embree(const ray &ray) {
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

template <bool alpha_aware>
bool embree_tracer<alpha_aware>::any_hit(const ray &ray) {
	RTCIntersectContext context;
	rtcInitIntersectContext(&context);
	RTCRay em_ray = ray_convert_to_embree(ray);
	rtcOccluded1(em_scene, &context, &em_ray);

	//tfar is set to -inf if there is a hit
	return std::isinf(em_ray.tfar) && (em_ray.tfar) < 0;
}

template <bool alpha_aware>
triangle_intersection embree_tracer<alpha_aware>::closest_hit(const ray &ray) {
	RTCIntersectContext context;
	rtcInitIntersectContext(&context);

	RTCRayHit ray_hit;
	ray_hit.ray = ray_convert_to_embree(ray);
	ray_hit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
	if constexpr(alpha_aware) {
		while (true) {
			rtcIntersect1(em_scene, &context, &ray_hit);

			//Again just a 1-1 conversion from the embree returned struct to ours
			triangle_intersection closest;
			if (ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
				closest.t = ray_hit.ray.tfar;
				//u and v correspond to beta and gamma, just different names
				closest.beta = ray_hit.hit.u;
				closest.gamma = ray_hit.hit.v;
				closest.ref = ray_hit.hit.primID;
				diff_geom dg(closest, *scene);
				
				if (dg.opacity() < ALPHA_THRESHOLD) {
					const float eps = 0.001f;
					ray_hit.ray.tnear = ray_hit.ray.tfar + eps;
					ray_hit.ray.tfar = ray.t_max;
					ray_hit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
					continue;
				}
				return closest;
			}
			else {
				closest.t = FLT_MAX;
				//u and v correspond to beta and gamma, just different names
				closest.beta = ray_hit.hit.u;
				closest.gamma = ray_hit.hit.v;
				closest.ref = ray_hit.hit.primID;
				return closest;
			}
		}
	}
	else {	
		rtcIntersect1(em_scene, &context, &ray_hit);

		//Again just a 1-1 conversion from the embree returned struct to ours
		triangle_intersection closest;
		if(ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
			closest.t = ray_hit.ray.tfar;
		else
			closest.t = FLT_MAX;
		//u and v correspond to beta and gamma, just different names
		closest.beta = ray_hit.hit.u;
		closest.gamma = ray_hit.hit.v;
		closest.ref = ray_hit.hit.primID;

		return closest;
	}
}

//BVH Generation Callbacks

void* embvh_create_node(RTCThreadLocalAllocator alloc,
						unsigned int num_children,
						void *user_ptr) {
	auto &cb_data = *static_cast<bvh_callback_data*>(user_ptr);
	bbvh_node new_node;

	pthread_rwlock_wrlock(&cb_data.lock);
	cb_data.bvh_nodes.push_back(new_node);
	uint64_t index = cb_data.bvh_nodes.size() - 1;
	pthread_rwlock_unlock(&cb_data.lock);

	return reinterpret_cast<void*>(index);
}

void embvh_set_node_children(void *node_ptr, void **children,
							 unsigned int child_count,
							 void *user_ptr) {
	auto &cb_data = *static_cast<bvh_callback_data*>(user_ptr);
	uint32_t index = reinterpret_cast<uint64_t>(node_ptr);

	pthread_rwlock_wrlock(&cb_data.lock);
	cb_data.bvh_nodes[index].link_l = reinterpret_cast<uint64_t>(children[0]);
	cb_data.bvh_nodes[index].link_r = reinterpret_cast<uint64_t>(children[1]);
	pthread_rwlock_unlock(&cb_data.lock);
}

void embvh_set_node_bounds(void *node_ptr,
						   const struct RTCBounds **bounds,
						   unsigned int child_count, void *user_ptr) {
	auto &cb_data = *static_cast<bvh_callback_data*>(user_ptr);
	uint32_t index = reinterpret_cast<uint64_t>(node_ptr);

	aabb aabbs[child_count];
	for(int i = 0; i < child_count; i++) {
		aabbs[i].min.x = bounds[i]->lower_x;
		aabbs[i].min.y = bounds[i]->lower_y;
		aabbs[i].min.z = bounds[i]->lower_z;
		aabbs[i].max.x = bounds[i]->upper_x;
		aabbs[i].max.y = bounds[i]->upper_y;
		aabbs[i].max.z = bounds[i]->upper_z;
	}

	pthread_rwlock_wrlock(&cb_data.lock);
	cb_data.bvh_nodes[index].box_l = aabbs[0];
	cb_data.bvh_nodes[index].box_r = aabbs[1];
	pthread_rwlock_unlock(&cb_data.lock);
}

void* embvh_create_leaf(RTCThreadLocalAllocator allocator,
						const struct RTCBuildPrimitive *primitives,
						size_t primitive_count, void *user_ptr) {
	auto &cb_data = *static_cast<bvh_callback_data*>(user_ptr);
	bbvh_node new_node;
	new_node.tri_count(primitive_count);
	new_node.tri_offset(primitives[0].primID);

	pthread_rwlock_wrlock(&cb_data.lock);
	cb_data.bvh_nodes.push_back(new_node);
	// Not nice, but I can't change the callback signatures
	uint64_t index = cb_data.bvh_nodes.size() - 1;
	pthread_rwlock_unlock(&cb_data.lock);

	return reinterpret_cast<void*>(index);
}

void embvh_split_primitive(const struct RTCBuildPrimitive *primitive,
						   unsigned int dimension,
						   float position,
						   struct RTCBounds *bounds_l,
						   struct RTCBounds *bounds_r,
						   void *user_ptr) {
	bounds_l->lower_x = primitive->lower_x;
	bounds_r->lower_x = primitive->lower_x;
	bounds_l->lower_y = primitive->lower_y;
	bounds_r->lower_y = primitive->lower_y;
	bounds_l->lower_z = primitive->lower_z;
	bounds_r->lower_z = primitive->lower_z;
	bounds_l->upper_x = primitive->upper_x;
	bounds_r->upper_x = primitive->upper_x;
	bounds_l->upper_y = primitive->upper_y;
	bounds_r->upper_y = primitive->upper_y;
	bounds_l->upper_z = primitive->upper_z;
	bounds_r->upper_z = primitive->upper_z;

	switch (dimension) {
	case 0:
		bounds_l->upper_x = position;
		bounds_r->lower_x = position;
		break;
	case 1:
		bounds_l->upper_y = position;
		bounds_r->lower_y = position;
		break;
	case 2:
		bounds_l->upper_z = position;
		bounds_r->lower_z = position;
		break;
	}
}

template struct embree_tracer<>;
template struct embree_tracer<true>;

#endif
#endif
#endif
