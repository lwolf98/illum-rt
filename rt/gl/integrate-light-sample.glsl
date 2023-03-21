define(VERSION, 460)
include(preamble.glsl)

uniform layout(rgba32f,binding=0) image2D camrays;
uniform layout(rgba32f,binding=1) image2D cam_hits;
uniform layout(rgba32f,binding=2) image2D framebuffer;
uniform layout(rgba32f,binding=3) image2D shadowrays;
uniform layout(rgba32f,binding=4) image2D shadow_hits;
uniform layout(r32f,binding=5) image2D pdf;
uniform layout(rgba32f,binding=6) image2D light_col;

void add_radiance(uint x, uint y, vec4 result) {
	vec4 before = imageLoad(framebuffer, ivec2(x, y));
	imageStore(framebuffer, ivec2(x, y), before + result);
}

vec3 albedo(ivec4 tri, vec4 hit, material mat) {
	if (mat.has_tex == 1) {
		vec2 tc = (1.0 - hit_beta(hit) - hit_gamma(hit)) * vertices[tri.x].tc
			+ hit_beta(hit) * vertices[tri.y].tc
			+ hit_gamma(hit) * vertices[tri.z].tc;
		return tex_lookup(tc, mat).rgb;
	}
	else
		return mat.albedo.rgb;
}

// those are from sampling.h and util.h

bool same_hemisphere(const vec3 N, const vec3 v) {
    return dot(N, v) > 0;
}

float fresnel_dielectric(float cos_wi, float ior_medium, float ior_material) {
    // check if entering or leaving material
    const float ei = cos_wi < 0.0 ? ior_material : ior_medium;
    const float et = cos_wi < 0.0 ? ior_medium : ior_material;
    cos_wi = clamp(abs(cos_wi), 0.0, 1.0);
    // snell's law
    const float sin_t = (ei / et) * sqrt(1.0 - cos_wi * cos_wi);
    // handle TIR
    if (sin_t >= 1.0) return 1.0;
	const float rev_sin2 = 1. - sin_t * sin_t;
    const float cos_t = sqrt(rev_sin2 > 0.0 ? rev_sin2 : 0.0);
    const float Rparl = ((et * cos_wi) - (ei * cos_t)) / ((et * cos_wi) + (ei * cos_t));
    const float Rperp = ((ei * cos_wi) - (et * cos_t)) / ((ei * cos_wi) + (et * cos_t));
    return (Rparl * Rparl + Rperp * Rperp) / 2;
}

float cos2_theta(float cos_t) {
    return cos_t*cos_t;
}
float sin2_theta(float cos_t) {
	float res = 1.0 - cos_t*cos_t;
    return res < 0.0 ? 0.0 : res;
}
float tan2_theta(float cos_t) {
    return sin2_theta(cos_t) / cos2_theta(cos_t);
}
float cdot(const vec3 a, const vec3 b) {
	return clamp(dot(a,b), 0, 1);
}

// those are from material (and are also replicated in the cuda variant)

vec3 lambertian_reflection(const vec3 w_o, const vec3 w_i, vec3 hit_ns, ivec4 tri, vec4 hit, material mat) {
	if (!same_hemisphere(w_i, hit_ns)) return vec3(0);
	return one_over_pi * albedo(tri, hit, mat);
}


#define sqr(x) ((x)*(x))
float ggx_d(const float NdotH, float roughness) {
    if (NdotH <= 0) return 0;
    const float tan2 = tan2_theta(NdotH);
    if (isinf(tan2)) return 0;
    const float a2 = sqr(roughness);
    return a2 / (pi * sqr(sqr(NdotH)) * sqr(a2 + tan2));
}

float ggx_g1(const float NdotV, float roughness) {
    if (NdotV <= 0) return 0.;
    const float tan2 = tan2_theta(NdotV);
    if (isinf(tan2)) return 0.;
    return 2. / (1. + sqrt(1. + sqr(roughness) * tan2));
}
#undef sqr
 
vec3 gtr2_coat_reflection(const vec3 w_o, const vec3 w_i, vec3 hit_ns, ivec4 tri, vec4 hit, material mat) {
	if (!same_hemisphere(hit_ns, w_i)) return vec3(0); // should be ng
    const float NdotV = cdot(hit_ns, w_o);
    const float NdotL = cdot(hit_ns, w_i);
    if (NdotV == 0.f || NdotV == 0.f) return vec3(0);
    const vec3 H = normalize(w_o + w_i);
    const float NdotH = cdot(hit_ns, H);
    const float HdotL = cdot(H, w_i);
    const float roughness = mat.roughness;
    const float F = fresnel_dielectric(HdotL, 1.f, mat.ior);
    const float D = ggx_d(NdotH, roughness);
    const float G = ggx_g1(NdotV, roughness) * ggx_g1(NdotL, roughness);
    const float microfacet = (F * D * G) / (4 * abs(NdotV) * abs(NdotL));
    return vec3(microfacet);
}

vec3 layered_gtr2(const vec3 w_o, const vec3 w_i, const vec3 hit_ns, ivec4 tri, vec4 hit, material mat) {
	const float F = fresnel_dielectric(abs(dot(hit_ns, w_o)), 1.0, mat.ior);
	vec3 diff = lambertian_reflection(w_o, w_i, hit_ns, tri, hit, mat);
	vec3 spec = gtr2_coat_reflection(w_o, w_i, hit_ns, tri, hit, mat);
	return (1.0-F)*diff + F*spec;
}

// actual kernel

void run(uint x, uint y) {
	uint id = y * w + x;
	vec4 hit = imageLoad(cam_hits, ivec2(x, y));
	vec4 shadow_hit = imageLoad(shadow_hits, ivec2(x, y));
	vec3 radiance = vec3(0);
	vec4 shadowray_dir = imageLoad(shadowrays, ivec2(x,y+h));
	if (valid_hit(hit) && shadowray_dir.w > 0 && !valid_hit(shadow_hit)) {
		// light color
		vec3 brightness = imageLoad(light_col, ivec2(x,y)).rgb;
		// brdf
		vec3 w_o = -imageLoad(camrays, ivec2(x,y+h)).xyz;
		vec3 w_i = shadowray_dir.xyz;
		ivec4 tri = triangles[hit_ref(hit)];
		vec3 ng = hit_ng(hit, tri);
		material mat = materials[tri.w];
		vec3 f = layered_gtr2(w_o, w_i, ng, tri, hit, mat);
		// dot
		float cos_theta = clamp(dot(w_i, ng), 0, 1);
		// combine
		radiance = brightness * f * cos_theta / imageLoad(pdf, ivec2(x,y)).x;
		float pdf_val = imageLoad(pdf, ivec2(x,y)).x;
	}
	add_radiance(x, y, vec4(radiance,1));
}

