define(VERSION, 460)
include(preamble.glsl)
include(random.glsl)

uniform layout(rgba32f,binding=0) image2D camrays;
uniform layout(rgba32f,binding=1) image2D hits;
uniform layout(rgba32f,binding=2) image2D framebuffer;
uniform layout(rgba32f,binding=3) image2D shadowrays;
uniform layout(r32f,binding=4) image2D pdf;

void add_radiance(uint x, uint y, vec4 result) {
	vec4 before = imageLoad(framebuffer, ivec2(x, y));
	imageStore(framebuffer, ivec2(x, y), before + result);
}

// those are from sampling.h and util.h

vec2 uniform_sample_disk(const vec2 xi) {
    const float r = sqrt(xi.x);
    const float theta = 2 * pi * xi.y;
    return r * vec2(cos(theta), sin(theta));
}

vec3 uniform_sample_hemisphere(const vec2 xi) {
    const float z = xi.x;
	const float r0 = 1 - z * z;
    const float r = sqrt(r0 < 0 ? -r0 : r0);
    const float phi = 2 * pi * xi.y;
    return vec3(r * cos(phi), r * sin(phi), z);
}

bool same_hemisphere(const vec3 N, const vec3 v) {
    return dot(N, v) > 0;
}

void flip_normals_to_ray(inout vec3 normal, const vec3 ray_dir) {
	if (same_hemisphere(ray_dir, normal))
		normal *= -1;
}

vec3 align(const vec3 v, const vec3 axis) {
    const float s = axis.z < 0 ? -1 : 1;//copysign(1.f, axis.z);
    const vec3 w = vec3(v.x, v.y, v.z * s);
    const vec3 h = vec3(axis.x, axis.y, axis.z + s);
    const float k = dot(w, h) / (1.f + (axis.z < 0 ? -axis.z : axis.z));
    return k * h - w;
}

// actual kernel

void run(uint x, uint y) {
	uint id = y * w + x;
	vec4 hit = imageLoad(hits, ivec2(x, y));
	vec3 w_i = vec3(0,0,0);
	vec3 org = vec3(0,0,0);
	float tmax = -FLT_MAX;
	if (valid_hit(hit)) {
		ivec4 tri = triangles[hit_ref(hit)];
		material m = materials[tri.w];
		if (m.emissive.rgb != vec3(0,0,0))
			add_radiance(x, y, m.emissive);
		else {
			vec2 xi = random_float2(id);
			vec3 sampled_dir = uniform_sample_hemisphere(xi);
			vec3 ng = hit_ng(hit, tri);
			vec3 cam_dir = imageLoad(camrays, ivec2(x, h+y)).rgb;
			flip_normals_to_ray(ng, cam_dir);
			w_i = align(sampled_dir, ng);
			org = imageLoad(camrays, ivec2(x, y)).rgb + hit_t(hit) * cam_dir;
			tmax = FLT_MAX;
		}
	}
	imageStore(shadowrays, ivec2(x, y), vec4(org, 0.001));
	imageStore(shadowrays, ivec2(x, h+y), vec4(w_i, tmax));
	imageStore(shadowrays, ivec2(x, 2*h+y), vec4(vec3(1)/w_i, 1));
	imageStore(pdf, ivec2(x, y), vec4(one_over_2pi,0,0,0));
}
