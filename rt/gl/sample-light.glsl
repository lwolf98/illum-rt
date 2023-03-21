define(VERSION, 460)
include(preamble.glsl)
include(random.glsl)

uniform layout(rgba32f,binding=0) image2D camrays;
uniform layout(rgba32f,binding=1) image2D hits;
uniform layout(rgba32f,binding=2) image2D framebuffer;
uniform layout(rgba32f,binding=3) image2D shadowrays;
uniform layout(r32f,binding=4)    image2D pdfs;
uniform layout(r32f,binding=5)    image1D lights_f;
uniform layout(r32f,binding=6)    image1D lights_cdf;
uniform layout(rgba32i,binding=7)iimage1D tri_lights;
uniform layout(rgba32f,binding=8) image2D light_col;
uniform float integral_1spaced;
uniform int lights;

const float eps = 1e-4f; // see rt.h

void add_radiance(uint x, uint y, vec4 result) {
	vec4 before = imageLoad(framebuffer, ivec2(x, y));
	imageStore(framebuffer, ivec2(x, y), before + result);
}

// those are from sampling.h and util.h

bool same_hemisphere(const vec3 N, const vec3 v) {
    return dot(N, v) > 0;
}

// returns baryzentric coordinates
vec2 uniform_sample_triangle(const vec2 xi) {
    const float su0 = sqrt(xi.x);
    return vec2(1.0 - su0, xi.y * su0);
}

void flip_normals_to_ray(inout vec3 normal, const vec3 ray_dir) {
	if (same_hemisphere(ray_dir, normal))
		normal *= -1;
}

// this is from scene.cpp

void sample_Li(in uint x, in uint y, in int index, in vec4 hit, in vec2 xi,
			   out vec3 out_dir, out vec3 out_pos, out float out_tmax, out vec3 out_lcol, out float out_pdf) {
	ivec4 l_tri = imageLoad(tri_lights, index);
	vec3 cam_d  = imageLoad(camrays, ivec2(x, h+y)).rgb;
	vec3 from   = imageLoad(camrays, ivec2(x, y)).xyz + hit_t(hit) * cam_d;
	vec2 bc     = uniform_sample_triangle(xi);
	vec3 target = vec3((1.0-bc.x-bc.y) * vertices[l_tri.x].pos  + bc.x * vertices[l_tri.y].pos  + bc.y * vertices[l_tri.z].pos);
	vec3 n      = vec3((1.0-bc.x-bc.y) * vertices[l_tri.x].norm + bc.x * vertices[l_tri.y].norm + bc.y * vertices[l_tri.z].norm);
	vec3 w_i    = target - from;
	
	float area  = 0.5 * length(cross(vec3(vertices[l_tri.y].pos-vertices[l_tri.x].pos),
									vec3(vertices[l_tri.z].pos-vertices[l_tri.x].pos)));
	material mat = materials[l_tri.w];
	vec3 col = mat.emissive.rgb;
	
	float tmax = length(w_i);
	w_i /= tmax;
	tmax -= eps;
	out_pos = from;
	out_dir = w_i;
	out_tmax = tmax;

	float cos_theta_light = dot(n,-w_i);
	if (cos_theta_light <= 0.0) {
		out_lcol = vec3(0);
		out_pdf = 0;
		return;
	}
	out_lcol = col;
	out_pdf = tmax*tmax/(cos_theta_light * area);
	return;
}

// other

int lower_bound(int n, float v) {
	int count = n;
	int first = 0;
	int i, step;
	while (count > 0) {
		i = first;
		step = count/2;
		i += step;
		if (imageLoad(lights_cdf, i).r < v) {
			first = ++i;
			count -= step+1;
		}
		else
			count = step;
	}
	return i;
}

// actual kernel

void run(uint x, uint y) {
	uint id = y * w + x;
	vec4 hit = imageLoad(hits, ivec2(x, y));
	vec3 shadow_dir = vec3(0,0,0);
	vec3 shadow_org = vec3(0,0,0);
	float shadow_tmax = -FLT_MAX;
	float pdf = 0;
	vec3 col = vec3(0);
	if (valid_hit(hit)) {
		ivec4 hit_tri = triangles[hit_ref(hit)];
		material m = materials[hit_tri.w];
		if (m.emissive.rgb != vec3(0,0,0))
			add_radiance(x, y, vec4(m.emissive.rgb, 0));
		else {
			// sample light index
			float xi_light = random_float2(id).x;
			int index = lower_bound(lights, xi_light);
			index = index > 0 ? index - 1 : index; // for xi.x==0
			float l_pdf = imageLoad(lights_f, index).r / integral_1spaced;	
			// sample light surface
			vec2 xi = random_float2(id);
			float a_pdf = 0, r_tm;
			vec3 l_col, r_d, r_o;
			sample_Li(x, y, index, hit, xi,
					  r_d, r_o, r_tm, l_col, a_pdf);
			if (l_col != vec3(0)) {
				shadow_org = r_o;
				shadow_dir = r_d;
				shadow_tmax = r_tm;
				col = l_col;
			}
			pdf = a_pdf*l_pdf;
		}
	}
	imageStore(shadowrays, ivec2(x, y),   vec4(shadow_org, 0.001));
	imageStore(shadowrays, ivec2(x, h+y), vec4(shadow_dir, shadow_tmax));
	imageStore(shadowrays, ivec2(x, 2*h+y), vec4(vec3(1)/shadow_dir, 1));
	imageStore(pdfs,       ivec2(x, y), vec4(pdf,0,0,0));
	imageStore(light_col,  ivec2(x, y), vec4(col,0));
}
