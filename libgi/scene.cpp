#include "scene.h"

#include "global-context.h"
#include "color.h"
#include "util.h"
#ifndef RTGI_SKIP_DIRECT_ILLUM
#include "sampling.h"
#endif
#ifndef RTGI_SKIP_SKY
#include "framebuffer.h"
#endif

#include <vector>
#include <iostream>
#include <fstream>
#include <map>
#include <filesystem>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/mesh.h>
#include <assimp/material.h>

// debug
// #include <png++/png.hpp>

#ifdef RTGI_WAND7
#include <MagickWand/MagickWand.h>
#else
#include <wand/MagickWand.h>
#endif

using namespace glm;
using namespace std;

inline vec3 to_glm(const aiVector3D& v) { return vec3(v.x, v.y, v.z); }

static bool verbose_scene = false;

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
	MagickExportImagePixels(img, 0, 0, tex->w, tex->h, "RGBA", FloatPixel, (void*)tex->texel);
	#pragma omp parallel for
	for (int i = 0; i < tex->w*tex->h; ++i) {
		tex->texel[i].x = pow(tex->texel[i].x, 2.2f);
		tex->texel[i].y = pow(tex->texel[i].y, 2.2f);
		tex->texel[i].z = pow(tex->texel[i].z, 2.2f);
		
		if (!has_alpha_channel) tex->texel[i].w = 1;
	}
	
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



texture2d<vec3>* load_hdr_image3f(const std::filesystem::path &path) {
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


void scene::add(const filesystem::path& path, const std::string &name, const mat4 &trafo) {
	// find file
	filesystem::path modelpath;
	if (path.is_relative())
		for (auto p : modelpaths)
			if (exists(p / path)) {
				modelpath = p / path;
				break;
			}
	if (modelpath == "")
		throw std::runtime_error("Model " + path.string() + " not found in any search directory");
    // load from disk
    Assimp::Importer importer;
	unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals;  // | aiProcess_FlipUVs  // TODO assimp
    const aiScene* scene_ai = importer.ReadFile(modelpath.string(), flags);
    if (!scene_ai) // handle error
        throw std::runtime_error("ERROR: Failed to load file: " + modelpath.string() + "!");

	// todo: store indices prior to adding anything to allow "transform-last"

#ifndef RTGI_SKIP_BRDF
	// initialize brdfs
	if (brdfs.empty() || brdfs.count("default") == 0) {
		brdfs["default"] = brdfs["lambert"] = new lambertian_reflection;
	}
#endif

	// load materials
	unsigned material_offset = materials.size();
    for (uint32_t i = 0; i < scene_ai->mNumMaterials; ++i) {
		::material material;
        aiString name_ai;
		aiColor3D col;
		auto mat_ai = scene_ai->mMaterials[i];
        mat_ai->Get(AI_MATKEY_NAME, name_ai);
		if (name != "") material.name = name + "/" + name_ai.C_Str();
		else            material.name = name_ai.C_Str();
		
		vec3 kd(0), ks(0), ke(0);
		float tmp;
		if (mat_ai->Get(AI_MATKEY_COLOR_DIFFUSE,  col) == AI_SUCCESS) kd = vec4(col.r, col.g, col.b, 1.0f);
		if (mat_ai->Get(AI_MATKEY_COLOR_SPECULAR, col) == AI_SUCCESS) ks = vec4(col.r, col.g, col.b, 1.0f);
		if (mat_ai->Get(AI_MATKEY_COLOR_EMISSIVE, col) == AI_SUCCESS) ke = vec4(col.r, col.g, col.b, 1.0f);
		if (mat_ai->Get(AI_MATKEY_SHININESS,      tmp) == AI_SUCCESS) material.roughness = roughness_from_exponent(tmp);
		if (mat_ai->Get(AI_MATKEY_REFRACTI,       tmp) == AI_SUCCESS) material.ior = tmp;
		if (material.ior == 1.0f) material.ior = 1.3;
		if (luma(kd) > 1e-4) material.albedo = kd;
		else                 material.albedo = ks;
		material.albedo = pow(material.albedo, vec3(2.2f, 2.2f, 2.2f));
		material.emissive = ke;
		
		if (mat_ai->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString path_ai;
			mat_ai->GetTexture(aiTextureType_DIFFUSE, 0, &path_ai);
			filesystem::path p = modelpath.parent_path() / path_ai.C_Str();

			if (mat_ai->GetTextureCount(aiTextureType_OPACITY) > 0) {
				aiString mask_path_ai;
				mat_ai->GetTexture(aiTextureType_OPACITY, 0, &mask_path_ai);
				filesystem::path mask_path = modelpath.parent_path() / mask_path_ai.C_Str();
				material.albedo_tex = load_image4f(p, &mask_path);
			} else {
				material.albedo_tex = load_image4f(p);
			}
			textures.push_back(material.albedo_tex);
		}


#ifndef RTGI_SKIP_BRDF
		material.brdf = brdfs["default"];
#endif
	
		materials.push_back(material);
	}

    // load meshes
    for (uint32_t i = 0; i < scene_ai->mNumMeshes; ++i) {
        const aiMesh *mesh_ai = scene_ai->mMeshes[i];
		uint32_t material_id = scene_ai->mMeshes[i]->mMaterialIndex + material_offset;
		uint32_t index_offset = vertices.size();
		std::string object_name = mesh_ai->mName.C_Str();
		objects.push_back({object_name, (unsigned)triangles.size(), (unsigned)(triangles.size()+mesh_ai->mNumFaces), material_id});
		
		for (uint32_t i = 0; i < mesh_ai->mNumVertices; ++i) {
			vertex vertex;
			vertex.pos = to_glm(mesh_ai->mVertices[i]);
			vertex.norm = to_glm(mesh_ai->mNormals[i]);
			if (mesh_ai->HasTextureCoords(0))
				vertex.tc = vec2(to_glm(mesh_ai->mTextureCoords[0][i]));
			else
				vertex.tc = vec2(0,0);
			vertices.push_back(vertex);
			scene_bounds.grow(vertex.pos);
		}
 
		for (uint32_t i = 0; i < mesh_ai->mNumFaces; ++i) {
			const aiFace &face = mesh_ai->mFaces[i];
			if (face.mNumIndices == 3) {
				triangle triangle;
				triangle.a = face.mIndices[0] + index_offset;
				triangle.b = face.mIndices[1] + index_offset;
				triangle.c = face.mIndices[2] + index_offset;
				triangle.material_id = material_id;
				triangles.push_back(triangle);
			}
			else
				std::cout << "WARN: Mesh: skipping non-triangle [" << face.mNumIndices << "] face (that the ass imp did not triangulate)!" << std::endl;
		}
	}
}
	
#ifndef RTGI_SKIP_DIRECT_ILLUM
void scene::compute_light_distribution() {
	std::vector<object> light_geom;
	unsigned prims = 0;
	for (auto o : objects)
		if (materials[o.material_id].emissive != vec3(0)) {
			prims += o.end-o.start;
			light_geom.push_back(o);
		}

#ifndef RTGI_SKIP_SKY
	if (prims == 0 && !sky) {
		std::cerr << "WARNING: There is neither emissive geometry nor a skylight" << std::endl;
		return;
	}
#else
	if (prims == 0) {
		std::cerr << "WARNING: There is no emissive geometry" << std::endl;
		return;
	}
#endif
	if (verbose_scene) cout << "light distribution of " << prims << " triangles" << endl;
	for (auto l : lights) delete l;
	lights.clear();
	int n = prims;
#ifndef RTGI_SKIP_SKY
	if (sky) {
		n++;
		sky->build_distribution();
		sky->scene_bounds(scene_bounds);
	}
#endif
	lights.resize(n);
	std::vector<float> power(n);
	int l = 0;
	for (auto g : light_geom) {
		for (int i = g.start; i < g.end; ++i) {
			lights[l] = new trianglelight(*this, i);
			power[l] = luma(lights[l]->power());
			l++;
		}
	}
#ifndef RTGI_SKIP_SKY
	if (sky) {
		lights[n-1] = sky;
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
	vec3 n      = (1.0f-bc.x-bc.y)*a.norm + bc.x*b.norm + bc.y*c.norm;
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
    float v = acosf(ray.d.y) / M_PI;
    assert(std::isfinite(u));
    assert(std::isfinite(v));
    return tex->sample(u, v) * intensity_scale;
}

vec3 skylight::power() const {
	return vec3(pi * scene_radius * scene_radius * distribution->unit_integral());
}


#endif
