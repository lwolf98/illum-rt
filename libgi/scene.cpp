#include "scene.h"

#include "asset-import/asset-import.h"

#include "global-context.h"
#include "color.h"
#include "util.h"
#include "subdivision.h"
#ifndef RTGI_SKIP_DIRECT_ILLUM
#include "sampling.h"
#endif
#ifndef RTGI_SKIP_SKY
#include "framebuffer.h"
#endif

#include <algorithm>
#include <cctype>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include <filesystem>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/mesh.h>
#include <assimp/material.h>

#include <glm/gtx/matrix_transform_2d.hpp>

// debug
// #include <png++/png.hpp>

#ifdef RTGI_WAND7
#include <MagickWand/MagickWand.h>
#else
#include <wand/MagickWand.h>
#endif

using glm::mat3, glm::mat4;
using namespace std;

static bool verbose_scene = false;

void scene::add_modelpath(const std::filesystem::path &p) {
	if (p.begin() != p.end() && *p.begin() == "~") {
		filesystem::path mod = getenv("HOME");
		for (auto it = ++p.begin(); it != p.end(); ++it)
			mod /= *it;
		modelpaths.push_back(mod);
	}
	else
		modelpaths.push_back(p);
}

void scene::remove_modelpath(const std::filesystem::path &p) {
	if (p.begin() != p.end() && *p.begin() == "~") {
		filesystem::path mod = getenv("HOME");
		for (auto it = ++p.begin(); it != p.end(); ++it)
			mod /= *it;
		remove(modelpaths.begin(), modelpaths.end(), p);
	}
	else
		remove(modelpaths.begin(), modelpaths.end(), p);
}

static filesystem::path find_model(const filesystem::path &path) {
	if (path.is_relative()) {
		for (auto p : rc->scene.modelpaths)
			if (exists(p / path))
				return p / path;
	}
	return path;
}

void magickwand_error(MagickWand *wand, bool crash) {
	char *description;
	ExceptionType severity;
	description=MagickGetException(wand,&severity);
	cerr << (GetMagickModule()) << ": " << description << endl;
	MagickRelinquishMemory(description);
	if (crash)
		exit(1);
}

texture2d<vec3>* load_image3f(const std::filesystem::path &path, bool crash_on_error) {
	if (verbose_scene) cout << "loading texture " << path << endl;
	MagickWandGenesis();
	MagickWand *img = NewMagickWand();
	int status = MagickReadImage(img, path.c_str());
	if (status == MagickFalse) {
		magickwand_error(img, crash_on_error);
		return nullptr;
	}
	MagickFlipImage(img);
	texture2d<vec3> *tex = new texture2d<vec3>;
	tex->name = path;
	tex->path = path;
	tex->w = MagickGetImageWidth(img);
	tex->h = MagickGetImageHeight(img);
	tex->texel = new vec3[tex->w*tex->h];
	MagickExportImagePixels(img, 0, 0, tex->w, tex->h, "RGB", FloatPixel, (void*)tex->texel);
	#pragma omp parallel for
	for (int i = 0; i < tex->w*tex->h; ++i)
		tex->texel[i] = pow(tex->texel[i], vec3(2.2f, 2.2f, 2.2f));
	DestroyMagickWand(img);
	MagickWandTerminus();
	return tex;
}

/*! Loads an image and the corresponding mask if the image has no alpha channel and the mask
 *  is provided. The texture mask is then integrated into the image data such that it represents the alpha value. 
 */
texture2d<vec4>* load_image4f(const std::filesystem::path &path, const std::filesystem::path *mask_path,  bool crash_on_error) {
	if (verbose_scene) cout << "loading texture " << path << endl;
	MagickWandGenesis();
	MagickWand *img = NewMagickWand();
	int status = MagickReadImage(img, path.c_str());
	if (status == MagickFalse) {
		magickwand_error(img, crash_on_error);
		return nullptr;
	}
	MagickFlipImage(img);
	texture2d<vec4> *tex = new texture2d<vec4>;
	tex->name = path;
	tex->path = path;
	tex->w = MagickGetImageWidth(img);
	tex->h = MagickGetImageHeight(img);
	tex->texel = new vec4[tex->w*tex->h];
	
	MagickBooleanType has_alpha_channel = MagickGetImageAlphaChannel(img);
	ColorspaceType colorspace = MagickGetImageColorspace(img);
	MagickExportImagePixels(img, 0, 0, tex->w, tex->h, "RGBA", FloatPixel, (void*)tex->texel);
	if (colorspace == RGBColorspace || colorspace == GRAYColorspace) {
		#pragma omp parallel for
		for (int i = 0; i < tex->w*tex->h; ++i) {
			tex->texel[i].x = pow(tex->texel[i].x, 2.2f);
			tex->texel[i].y = pow(tex->texel[i].y, 2.2f);
			tex->texel[i].z = pow(tex->texel[i].z, 2.2f);

			if (!has_alpha_channel) tex->texel[i].w = 1;
		}
	}
	else if (colorspace == sRGBColorspace) {
		#pragma omp parallel for
		for (int i = 0; i < tex->w*tex->h; ++i) {
			tex->texel[i].x = tex->texel[i].x;
			tex->texel[i].y = tex->texel[i].y;
			tex->texel[i].z = tex->texel[i].z;

			if (!has_alpha_channel) tex->texel[i].w = 1;
		}
	}
	else
		throw std::logic_error(std::string("Unsupported colorspace in texture ") + path.string());
	
	if (!has_alpha_channel && mask_path) {
		MagickWand *mask_image = NewMagickWand();
		int status = MagickReadImage(mask_image, mask_path->c_str());
		if (status == MagickFalse)
			magickwand_error(mask_image, crash_on_error);
		
		MagickFlipImage(mask_image);
		
		int w = MagickGetImageWidth(mask_image);
		int h = MagickGetImageHeight(mask_image);

		float *mask = new float[w * h];
		status = MagickExportImagePixels(mask_image, 0, 0, w, h, "I", FloatPixel, (void*)mask);
		if (status == MagickFalse)	
			magickwand_error(mask_image, crash_on_error);
		
		#pragma omp parallel for
		for (int i = 0; i < w*h; ++i)
			tex->texel[i].w = mask[i];
		
		DestroyMagickWand(mask_image);
	}

	DestroyMagickWand(img);
	MagickWandTerminus();
	return tex;
}



texture2d<vec3>* load_hdr_image3f(const std::filesystem::path &given_path) {
	auto path = find_model(given_path);
	cout << "loading hdr texture from floats-file " << path << endl;
	ifstream in;
	in.open(path, ios::in | ios::binary);
	if (!in.is_open())
		throw runtime_error("Cannot open file '" + path.string() + "' for hdr floats texture.");
	texture2d<vec3> *tex = new texture2d<vec3>;
	tex->name = path;
	tex->path = path;
	in.read(((char*)&tex->w), sizeof(int));
	in.read(((char*)&tex->h), sizeof(int));
	tex->texel = new vec3[tex->w * tex->h];
	in.read(((char*)tex->texel), tex->w * tex->h * sizeof(vec3));
	if (!in.good())
		throw runtime_error("Error loading data from '" + path.string() + "' for hdr floats texture.");
	return tex;
}

void scene::add(const filesystem::path& path, const std::string &name, const mat4 &trafo, const uint subdiv_level, const bool subdiv_type_patches) {
	#ifndef RTGI_SKIP_BRDF
		// initialize brdfs
		if (brdfs.empty() || brdfs.count("default") == 0) {
			brdfs["default"] = brdfs["lambert"] = new lambertian_reflection;
		}
	#endif

	// find file
	filesystem::path modelpath = find_model(path);
	if (modelpath == "")
		throw std::runtime_error("Model " + path.string() + " not found in any search directory");

	/**
	 * asset_importer importer;
	 * if ("usd*")
	 * 		importer = new usd_importer;
	 * else if ("objx")
	 * 		importer = new objx_importer;
	 * else
	 * 		importer = new assimp_importer;
	 * 
	 * importer.load_scene(filepath)
	 * importer.import(scene)
	 * 
	 */
	std::string extension = modelpath.extension().c_str();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char c) { return std::tolower(c); });
		
	if (extension.substr(0, 4) == ".usd") {
		import::usd_importer imp(name, trafo, subdiv_level, subdiv_type_patches);
		imp.load_scene(modelpath);
		imp.import(*this);
	}
	else if (extension == ".objx") {
		import::objx_importer imp(name, trafo, subdiv_level, subdiv_type_patches);
		imp.load_scene(modelpath);
		imp.import(*this);
	}
	else {
		import::assimp_importer imp(name, trafo, subdiv_level, subdiv_type_patches);
		imp.load_scene(modelpath);
		imp.import(*this);
	}
}
	
#ifndef RTGI_SKIP_DIRECT_ILLUM
void scene::find_light_geometry() {
	for (int i = 0; i < triangles.size(); ++i) {
		// TODO: check if valid mat_id only required to allow dummy tris for referencing subd patches
		// remove and find different solution for final version
		uint32_t mat_id = triangles[i].material_id;
		if (mat_id > materials.size()-1)
			continue;
			
		if (auto mat = materials[mat_id]; mat.emissive != vec3(0))
			lights.push_back(new trianglelight(*this, i));

	}
}

void scene::compute_light_distribution() {
#ifndef RTGI_SKIP_SKY
	if (lights.size() == 0 && !sky) {
		std::cerr << "WARNING: There is neither emissive geometry nor a skylight" << std::endl;
		return;
	}
#else
	if (lights.size() == 0) {
		std::cerr << "WARNING: There is no emissive geometry" << std::endl;
		return;
	}
#endif
	if (verbose_scene) cout << "light distribution of " << lights.size() << " triangles" << endl;
	int n = lights.size();
#ifndef RTGI_SKIP_SKY
	// TODO move sky handling outside -> add_sky
	if (sky) {
		n++;
		sky->build_distribution();
		sky->scene_bounds(scene_bounds);
		if (verbose_scene)
			std::cout << "Skylight power " << sky->power() << std::endl;
	}
#endif
	std::vector<float> power(n);
	for (int l = 0; l < lights.size(); ++l)
		power[l] = luma(lights[l]->power());
#ifndef RTGI_SKIP_SKY
	if (sky) {
		lights.push_back(sky);
		power[n-1] = sky->power().x;
	}
#endif
#ifndef RTGI_SKIP_LIGHT_SOURCE_SAMPLING
// 	light_distribution = new distribution_1d(std::move(power));	
	light_distribution = new distribution_1d(power);	
	light_distribution->debug_out("/tmp/light-dist");
#endif
}
#endif

scene::~scene() {
	if(!rc->platform)
		delete rt;
	for (auto *x : textures)
		delete x;
#ifndef RTGI_SKIP_BRDF
	brdfs.erase("default");
	for (auto [str,brdf] : brdfs)
		delete brdf;
#endif
}

vec3 scene::normal(const triangle &tri) const {
	const vec3 &a = vertices[tri.a].pos;
	const vec3 &b = vertices[tri.b].pos;
	const vec3 &c = vertices[tri.c].pos;
	vec3 e1 = normalize(b-a);
	vec3 e2 = normalize(c-a);
	return cross(e1, e2);
}

void scene::release_rt() {
	rt = nullptr;
}

void scene::use(individual_ray_tracer *new_rt) {
	assert(rc->platform == nullptr);
	delete rt;
	rt = new_rt;
}

void scene::print_memory_stats() {
	uint64_t mesh = 0;
	mesh += sizeof(vertex)*vertices.size();
	mesh += sizeof(triangle)*triangles.size();
	mesh += sizeof(::material)*materials.size();
	uint64_t mesh_r = 0;
	mesh_r += sizeof(vertex)*vertices.capacity();
	mesh_r += sizeof(triangle)*triangles.capacity();
	mesh_r += sizeof(::material)*materials.capacity();
	uint64_t texs = 0;
	for (auto &m : materials)
		if (m.albedo_tex)
			texs += m.albedo_tex->size_in_bytes();
#ifndef RTGI_SKIP_BRDF
	uint64_t lights = 0, lights_r = 0;
#ifndef RTGI_SKIP_SKY
	if (sky) {
		lights += sky->tex->size_in_bytes();
		lights_r += sky->tex->size_in_bytes();
		auto [s,c] = sky->distribution->size_in_bytes();
		lights += s;
		lights_r += c;
	}
#endif
	lights += this->lights.size() * (sizeof(light) + sizeof(light*));
	lights_r += this->lights.capacity() * (sizeof(light) + sizeof(light*));
#endif

	auto format = [](uint64_t mem) {
		vector<string> suffixes = { "", "K", "M", "G" };
		int s = 0;
		while (mem > 10000) {
			mem /= 1024;
			s++;
			if (s == suffixes.size()-1) break;
		}
		ostringstream oss; oss << mem << " " << suffixes[s];
		return oss.str();
	};
	cout << "Scene memory consumption" << endl;
	cout << "    meshes:   " << setw(10) << format(mesh) << " (" << format(mesh_r) << ")" << endl;
	cout << "    textures: " << setw(10) << format(texs) << endl;
#ifndef RTGI_SKIP_BRDF
	cout << "    lights:   " << setw(10) << format(lights) << " (" << format(lights_r) << ")" << endl;
#endif
}


#ifndef RTGI_SKIP_BRDF
vec3 pointlight::power() const {
	return 4*pi*col;
}
#endif
#ifndef RTGI_SKIP_LIGHT_SOURCE_SAMPLING

tuple<ray, vec3, float> pointlight::sample_Li(const diff_geom &from, const vec2 &xis) const {
	vec3 to_light = pos - from.x;
	float tmax = length(to_light);
	to_light /= tmax;
	ray r(from.x, to_light);
	r.length_exclusive(tmax);
	vec3 c = col / (tmax*tmax);
	return { r, c, 1.0f };
}

tuple<ray, vec3, vec3, float> pointlight::sample_Le(const vec2 &xis1, const vec2 &xis2) const {
	// Compare with pbrt Pointlights:
	// pbrt3/955
	ray r(pos, uniform_sample_sphere(xis1));
	return {r, col, r.d, 1.f*uniform_sphere_pdf()};
}

#endif

/////

#ifndef RTGI_SKIP_DIRECT_ILLUM
trianglelight::trianglelight(const ::scene &scene, uint32_t i) : triangle(scene.triangles[i]), scene(scene) {
}

vec3 trianglelight::power() const {
#ifndef RTGI_SKIP_DIRECT_ILLUM_LIGHT_POWER_IMPL
	const vertex &a = scene.vertices[this->a];
	const vertex &b = scene.vertices[this->b];
	const vertex &c = scene.vertices[this->c];
	vec3 e1 = b.pos-a.pos;
	vec3 e2 = c.pos-a.pos;
	const material &m = scene.materials[this->material_id];
	return m.emissive * 0.5f * length(cross(e1,e2)) * pi;
#else
	// todo: compute power emitted by this light
	return vec3(0);
#endif
}

#ifndef RTGI_SKIP_LIGHT_SOURCE_SAMPLING
tuple<ray, vec3, float> trianglelight::sample_Li(const diff_geom &from, const vec2 &xis) const {
	// pbrt3/845
	const vertex &a = scene.vertices[this->a];
	const vertex &b = scene.vertices[this->b];
	const vertex &c = scene.vertices[this->c];
	vec2 bc     = uniform_sample_triangle(xis);
	vec3 target = (1.0f-bc.x-bc.y)*a.pos + bc.x*b.pos + bc.y*c.pos;
	vec3 n      = normalize((1.0f-bc.x-bc.y)*a.norm + bc.x*b.norm + bc.y*c.norm);
	vec3 w_i    = target - from.x;
	
	float area = 0.5f * length(cross(b.pos-a.pos,c.pos-a.pos));
	const material &m = scene.materials[material_id];
	vec3 col = m.emissive;
	
	float tmax = length(w_i);
	w_i /= tmax;
	ray r(from.x, w_i);
	r.length_exclusive(tmax);
	
	// pbrt3/838
	float cos_theta_light = dot(n,-w_i);
	if (cos_theta_light <= 0.0f) return { r, vec3(0), 0.0f };
	float pdf = tmax*tmax/(cos_theta_light * area);
	return { r, col, pdf };
	
}

float trianglelight::pdf(const ray &r, const diff_geom &on_light) const {
	const vertex &a = scene.vertices[this->a];
	const vertex &b = scene.vertices[this->b];
	const vertex &c = scene.vertices[this->c];
	float area = 0.5f * length(cross(b.pos-a.pos,c.pos-a.pos));
	float d = length(on_light.x - r.o);
	float cos_theta_light = dot(on_light.ns, -r.d);
	if (cos_theta_light <= 0.0f) return 0.0f;
	float pdf = d*d/(cos_theta_light*area);
	return pdf;
}

tuple<ray, vec3, vec3, float> trianglelight::sample_Le(const vec2 &xis_pos, const vec2 &xis_dir) const {
	// Sample position on triangle:
	const vertex &a = scene.vertices[this->a];
	const vertex &b = scene.vertices[this->b];
	const vertex &c = scene.vertices[this->c];
	vec2 bc = uniform_sample_triangle(xis_pos);
	float area = 0.5f * length(cross(b.pos-a.pos,c.pos-a.pos));
	float pdf_pos = 1.f/area;

	vec3 target = (1.0f-bc.x-bc.y)*a.pos + bc.x*b.pos + bc.y*c.pos;
	vec3 n      = (1.0f-bc.x-bc.y)*a.norm + bc.x*b.norm + bc.y*c.norm;

	// Sample w:
	vec3 w_tan = cosine_sample_hemisphere(xis_dir);
	vec3 w = align(w_tan, n);
	//float cos_theta = cdot(w, n);
	float cos_theta = w_tan.z;
	float pdf_dir = cosine_hemisphere_pdf(cos_theta);

	const material &m = scene.materials[this->material_id];
	vec3 col = m.emissive;
	if (pdf_pos*pdf_dir == 0)
		cout << "WTF!!! " << dot(w, n) << endl;

	return {ray(target, w), col, n, pdf_pos*pdf_dir};
}
#endif
#endif

/////

#ifndef RTGI_SKIP_SKY

void skylight::build_distribution() {
	assert(tex);
	buffer<float> lum(tex->w, tex->h);
	lum.for_each([&](unsigned x, unsigned y) {
				 	lum(x,y) = luma(tex->value(x,y)) * sinf(pi*(y+0.5f)/tex->h);
				 });
	
// 	png::image<png::rgb_pixel> out(tex->w, tex->h);
// 	lum.for_each([&](int x, int y) {
// 						vec3 col = heatmap(lum(x,y));
// 						out[y][x] = png::rgb_pixel(col.x*255, col.y*255, col.z*255);
// 					});
// 	out.write("sky-luma.png");

	distribution = new distribution_2d(lum.data, lum.w, lum.h);
}

void skylight::scene_bounds(aabb box) {
	vec3 d = (box.max - box.min);
	scene_radius = sqrtf(dot(d,d));
}

tuple<ray, vec3, float> skylight::sample_Li(const diff_geom &from, const vec2 &xis) const {
	assert(tex && distribution);
	auto [uv,pdf] = distribution->sample(xis);
	float phi = uv.x * 2 * pi;
	float theta = uv.y * pi;
	float sin_theta = sinf(theta),
		  cos_theta = cosf(theta);
	if (pdf <= 0.0f || sin_theta <= 0.0f)
		return { ray(vec3(0), vec3(0)), vec3(0), 0.0f };
	vec3 w_i = vec3(sin_theta * cosf(phi), cos_theta, sin_theta * sinf(phi));
	ray r(from.x, w_i);
	pdf /= 2.0f * pi * pi * sin_theta;
	return { r, tex->sample(uv) * intensity_scale, pdf };
}

float skylight::pdf_Li(const ray &ray) const {
    const vec2 spherical = to_spherical(ray.d);
    const float sin_t = sinf(spherical.x);
    if (sin_t <= 0.f) return 0.f;
    return distribution->pdf(vec2(spherical.y * one_over_2pi, spherical.x * one_over_pi)) / (2.f * pi * pi * sin_t);
}

vec3 skylight::Le(const ray &ray) const {
    float u = atan2f(ray.d.z, ray.d.x) / (2 * M_PI);
    float v = theta_z(ray.d.y) / M_PI;
    assert(std::isfinite(u));
    assert(std::isfinite(v));
    return tex->sample(u, v) * intensity_scale;
}

tuple<ray, vec3, vec3, float> skylight::sample_Le(const vec2 &xis1, const vec2 &xis2) const {
	// Compare with pbrt Infinite Area Lights:
	// https://www.pbr-book.org/3ed-2018/Light_Transport_III_Bidirectional_Methods/The_Path-Space_Measurement_Equation#x2-InfiniteAreaLights
	// pbrt3/959

	// 1. Compute direction for skylight sample ray:
	// Find (u, v) sample coordinates in skylight texture
	auto [uv, map_pdf] = distribution->sample(xis1);
	float theta = uv[1] * pi;
	float phi = uv[0] * 2.f * pi;
	float cos_theta = cosf(theta);
	float sin_theta = sinf(theta);
	if (map_pdf <= 0.0f || sin_theta <= 0.0f)
		return { ray(vec3(0), vec3(0)), vec3(0), vec3(0), 0.f };
	vec3 w = -vec3(sin_theta * cosf(phi), cos_theta, sin_theta * sinf(phi));

	// 2. Compute origin for skylight sample ray:
	// Calculate base around -w
	vec3 v1 = -w;
	vec3 v2, v3;
	if (abs(v1.x) > abs(v1.y)) v2 = vec3(-v1.z, 0, v1.x) / std::sqrt(v1.x * v1.x + v1.z * v1.z);
	else                       v2 = vec3(0, v1.z, -v1.y) / std::sqrt(v1.y * v1.y + v1.z * v1.z);
	v3 = cross(v1, v2);

	vec2 cd = uniform_sample_disk(xis2);
	vec3 p_disk = scene_radius * (cd.x * v2 + cd.y * v3); //TODO: in the future take world_center offset into consideration (compare to pbrt3)
	ray r(p_disk + scene_radius * -w, w);

	// 3. Compute skylight ray PDFs:
	float pdf_dir = sin_theta == 0 ? 0 : map_pdf / (2 * pi * pi * sin_theta);
	float pdf_pos = 1 / (pi * scene_radius * scene_radius);

	vec3 col = tex->sample(uv) * intensity_scale;

	return {r, col, r.d, pdf_pos*pdf_dir};
}

vec3 skylight::power() const {
	return vec3(pi * scene_radius * scene_radius * distribution->unit_integral() * intensity_scale);
}


#endif
