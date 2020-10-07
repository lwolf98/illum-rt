#include "scene.h"

#include "color.h"
#include "util.h"
#ifndef RTGI_AXX
#include "sampling.h"
#endif

#include <vector>
#include <iostream>
#include <map>
#include <filesystem>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/mesh.h>
#include <assimp/material.h>

#ifdef RTGI_WAND7
#include <MagickWand/MagickWand.h>
#else
#include <wand/MagickWand.h>
#endif

using namespace glm;
using namespace std;

inline vec3 to_glm(const aiVector3D& v) { return vec3(v.x, v.y, v.z); }


void magickwand_error(MagickWand *wand, bool crash) {
	char *description;
	ExceptionType severity;
	description=MagickGetException(wand,&severity);
	cerr << (GetMagickModule()) << ": " << description << endl;
	MagickRelinquishMemory(description);
	if (crash)
		exit(1);
}

texture* load_image3f(const std::filesystem::path &path, bool crash_on_error) {
	cout << "loading texture " << path << endl;
	MagickWandGenesis();
	MagickWand *img = NewMagickWand();
	int status = MagickReadImage(img, path.c_str());
	if (status == MagickFalse) {
		magickwand_error(img, crash_on_error);
		return nullptr;
	}
	MagickFlipImage(img);
	texture *tex = new texture;
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

void scene::add(const filesystem::path& path, const std::string &name, const mat4 &trafo) {
    // load from disk
    Assimp::Importer importer;
//     std::cout << "Loading: " << path << "..." << std::endl;
	unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals;  // | aiProcess_FlipUVs  // TODO assimp
    const aiScene* scene_ai = importer.ReadFile(path.string(), flags);
    if (!scene_ai) // handle error
        throw std::runtime_error("ERROR: Failed to load file: " + path.string() + "!");

	// todo: store indices prior to adding anything to allow "transform-last"

	// initialize brdfs
	if (brdfs.empty() || brdfs.count("default") == 0) {
		brdfs["default"] = brdfs["lambert"] = new lambertian_reflection;
	}

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
		if (luma(kd) > 1e-4) material.albedo = kd;
		else                 material.albedo = ks;
		material.albedo = pow(material.albedo, vec3(2.2f, 2.2f, 2.2f));
		material.emissive = ke;
		
		if (mat_ai->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString path_ai;
			mat_ai->GetTexture(aiTextureType_DIFFUSE, 0, &path_ai);
			filesystem::path p = path.parent_path() / path_ai.C_Str();
			material.albedo_tex = load_image3f(p);
			textures.push_back(material.albedo_tex);
		}

		material.brdf = brdfs["default"];
	
		materials.push_back(material);
	}

    // load meshes
    for (uint32_t i = 0; i < scene_ai->mNumMeshes; ++i) {
        const aiMesh *mesh_ai = scene_ai->mMeshes[i];
		uint32_t material_id = scene_ai->mMeshes[i]->mMaterialIndex + material_offset;
		uint32_t index_offset = vertices.size();
		std::string object_name = mesh_ai->mName.C_Str();
		objects.push_back({object_name, (unsigned)triangles.size(), (unsigned)(triangles.size()+mesh_ai->mNumFaces), material_id});
#ifndef RTGI_AXX
		if (materials[material_id].emissive != vec3(0))
			light_geom.push_back(objects.back());
#endif
		
		for (uint32_t i = 0; i < mesh_ai->mNumVertices; ++i) {
			vertex vertex;
			vertex.pos = to_glm(mesh_ai->mVertices[i]);
			vertex.norm = to_glm(mesh_ai->mNormals[i]);
			if (mesh_ai->HasTextureCoords(0))
				vertex.tc = vec2(to_glm(mesh_ai->mTextureCoords[0][i]));
			else
				vertex.tc = vec2(0,0);
			vertices.push_back(vertex);
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
#ifndef RTGI_A03
			else
				std::cout << "WARN: Mesh: skipping non-triangle [" << face.mNumIndices << "] face (that the ass imp did not triangulate)!" << std::endl;
#endif
		}
	}
}
	
#ifndef RTGI_AXX
void scene::compute_light_distribution() {
	unsigned prims = 0; for (auto g : light_geom) prims += g.end-g.start;
	if (prims == 0) {
		std::cerr << "WARNING: There is no emissive geometry" << std::endl;
		return;
	}
	cout << "light distribution of " << prims << " triangles" << endl;
	for (auto l : lights) delete l;
	lights.clear();
	lights.resize(prims);
	std::vector<float> power(prims);
	int l = 0;
	for (auto g : light_geom) {
		for (int i = g.start; i < g.end; ++i) {
			lights[l] = new trianglelight(*this, i);
			power[l] = luma(lights[l]->power());
			l++;
		}
	}
// 	light_distribution = new distribution_1d(std::move(power));	
	light_distribution = new distribution_1d(power);	
	light_distribution->debug_out("/tmp/light-dist");
}
#endif

scene::~scene() {
	delete rt;
	for (auto *x : textures)
		delete x;
	brdfs.erase("default");
	for (auto [str,brdf] : brdfs)
		delete brdf;
}

vec3 scene::normal(const triangle &tri) const {
	const vec3 &a = vertices[tri.a].pos;
	const vec3 &b = vertices[tri.b].pos;
	const vec3 &c = vertices[tri.c].pos;
	vec3 e1 = normalize(b-a);
	vec3 e2 = normalize(c-a);
	return cross(e1, e2);
}

vec3 scene::sample_texture(const triangle_intersection &is, const triangle &tri, const texture *tex) const {
	vec2 tc = (1.0f-is.beta-is.gamma)*vertices[tri.a].tc + is.beta*vertices[tri.b].tc + is.gamma*vertices[tri.c].tc;
	return (*tex)(tc);
}


#ifndef RTGI_A02
vec3 pointlight::power() const {
	return 4*pi*col;
}
#endif

#ifndef RTGI_AXX


tuple<ray, vec3, float> pointlight::sample_Li(const diff_geom &from, const vec2 &xis) const {
	vec3 to_light = pos - from.x;
	float tmax = length(to_light);
	to_light /= tmax;
	ray r(from.x, to_light);
	r.length_exclusive(tmax);
	vec3 c = col / (tmax*tmax);
	return { r, c, 1.0f };
}

/////

trianglelight::trianglelight(const ::scene &scene, uint32_t i) : triangle(scene.triangles[i]), scene(scene) {
}

vec3 trianglelight::power() const {
	const vertex &a = scene.vertices[this->a];
	const vertex &b = scene.vertices[this->b];
	const vertex &c = scene.vertices[this->c];
	vec3 e1 = b.pos-a.pos;
	vec3 e2 = c.pos-a.pos;
	const material &m = scene.materials[this->material_id];
	return m.emissive * 0.5f * length(cross(e1,e2)) * pi;
}

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
