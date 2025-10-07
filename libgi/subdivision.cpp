#include <iostream>
#include <fstream>
#include <map>
#include "subdivision.h"
#include "libgi/timer.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/input.h>

using namespace glm;
using namespace std;

namespace subd {
	bool subd_debug = false;

	/* utility functionality */

	void normalize_edge_order(int &a, int &b) {
		if (a > b) {
			int tmp = b;
			b = a;
			a = tmp;
		}
	}


	/* edge implementation */

	bool edge::face_exists(int id) const {
		for (int i : face_ids)
			if (i == id)
				return true;

		return false;
	}


	/* edge_list implementation */

	uint64_t edge_list::hash(int a, int b) const {
		uint64_t hash_a = a;
		uint64_t hash_b = b;
		return hash_a << 32 | hash_b;
	}

	uint64_t edge_list::add(int a, int b, float sharpness) {
		assert(!initialized);
		normalize_edge_order(a, b);
		uint64_t hashval = hash(a, b);
		edges[hashval] = edge(a, b, sharpness);
		return hashval;
	}

	uint64_t edge_list::add(int a, int b) {
		return add(a, b, 0.f);
	}

	uint64_t edge_list::add(const edge &e) {
		uint64_t hashval = hash(e.v1, e.v2);
		edges[hashval] = e;
		return hashval;
	}

	uint64_t edge_list::get_key(int a, int b) const {
		normalize_edge_order(a, b);
		uint64_t hashval = hash(a, b);
		return edges.count(hashval) > 0 ? hashval : (uint64_t)-1;
	}

	uint64_t edge_list::get_key(const edge &e) const {
		return get_key(e.v1, e.v2);
	}

	int edge_list::get_index(uint64_t key) const {
		assert(initialized);
		assert(edge_indices.count(key) > 0);
		auto it = edge_indices.find(key);
		assert(it != edge_indices.end());
		return it->second;
	}

	int edge_list::get_index(int a, int b) const {
		normalize_edge_order(a, b);
		return get_index(hash(a, b));
	}

	int edge_list::get_index(const edge &e) const {
		return get_index(e.v1, e.v2);
	}

	edge& edge_list::get(uint64_t id) {
		return edges[id];
	}

	edge& edge_list::get(int a, int b) {
		return get(hash(a, b));
	}

	edge& edge_list::get_next(std::map<uint64_t, edge>::iterator &it) {
		assert(it != edges.end());
		edge &e = it->second;
		std::advance(it, 1);
		return e;
	}

	int edge_list::size() const {
		return edges.size();
	}

	bool edge_list::exists(int a, int b) const {
		return get_key(a, b) != (uint64_t)-1;
	}

	void edge_list::clear() {
		edges.clear();
		edge_indices.clear();
		initialized = false;
	}

	void edge_list::finish_init() {
		initialized = true;
		auto it = iterator();
		for (int i = 0; i < size(); i++) {
			edge_indices[it->first] = i;
			std::advance(it, 1);
		}
	}


	/* ctrl_vertex implementation */

	bool ctrl_vertex::edge_exists(uint64_t id) const {
		for (uint64_t key : edge_ids)
			if (key == id)
				return true;

		return false;
	}

	bool ctrl_vertex::face_exists(int id) const {
		for (int i : face_ids)
			if (i == id)
				return true;

		return false;
	}


	/* object implementation */

	// Load from Assimp
	void object::init_object(aiMesh *mesh_ai) {
		time_this_block(init_object);

		// Meta data
		mesh.has_normals = mesh_ai->HasNormals();
		mesh.has_texture = mesh_ai->HasTextureCoords(0);
		name = mesh_ai->mName.C_Str();

		// load control mesh vertices (ctrl_vertices)
		map<int, int> vert_map;
		for (uint32_t i = 0; i < mesh_ai->mNumVertices; ++i) {
			auto v = mesh_ai->mVertices[i];
			vec3 vert_pos = vec3(v.x, v.y, v.z);
			int vert_id = mesh.get_vert_id(vert_pos);
			bool is_new_vert = vert_id == -1;
			if (is_new_vert) {
				mesh.vertices.push_back(ctrl_vertex(vert_pos));
				vert_id = mesh.vertices.size()-1;
			}
			vert_map[i] = vert_id;
		}

		// load texture coordinates
		if (mesh.has_texture) {
			for (int i = 0; i < mesh_ai->mNumVertices; i++) {
				aiVector3D &tc = mesh_ai->mTextureCoords[0][i];
				mesh.tex_coords.push_back(vec2(tc.x, tc.y));
				mesh.vertices[vert_map[i]].tc = vec2(tc.x, tc.y);
				if (subd_debug)
					cout << "Input tc " << i << ": " << vec3(tc.x, tc.y, tc.z) << endl;
			}
		}
		else {
			//TODO: is this required?
			mesh.tex_coords.push_back(vec2(0, 0));
		}

		// load control mesh faces (ctrl_faces)
		for (uint32_t i = 0; i < mesh_ai->mNumFaces; ++i) {
			const aiFace &f = mesh_ai->mFaces[i];
			face face;

			if (f.mNumIndices < 3) continue; //TODO: message/warning for degenerate faces?
			for (uint32_t j = 0; j < f.mNumIndices; ++j) {
				vertex_config v;
				int assimp_vert_id = f.mIndices[j];
				v.pos = vert_map[assimp_vert_id];
				v.tc = assimp_vert_id;
				face.verts.push_back(v);
			}
			mesh.faces.push_back(face);
		}

		mesh.update();
	}

	// Load from USD with OpenUSD
	void object::init_object(const pxr::UsdGeomMesh &usd_mesh) {
		using namespace pxr;
		time_this_block(init_object);

		// Meta data
		mesh.has_normals = false; //TODO //mesh_ai->HasNormals();
		mesh.has_texture = true; //TODO //mesh_ai->HasTextureCoords(0);
		name = "USD object..."; //TODO //mesh_ai->mName.C_Str();
		material = "mtl..."; //TODO //load...

		// load control mesh vertices (ctrl_vertices)
		VtArray<GfVec3f> usd_points;
		usd_mesh.GetPointsAttr().Get(&usd_points);
		std::cout << "#points: " << usd_points.size() << std::endl;
		for (uint32_t i = 0; i < usd_points.size(); ++i) {
			auto v = usd_points[i];
			vec3 vert_pos = vec3(v[0], v[1], v[2]);
			mesh.vertices.push_back(ctrl_vertex(vert_pos));
		}

		// load texture coordinates
		UsdGeomPrimvarsAPI primvarsAPI(usd_mesh);
		UsdGeomPrimvar stPrimvar = primvarsAPI.GetPrimvar(TfToken("st"));
		VtArray<GfVec2f> uvs;
		mesh.has_texture = stPrimvar && stPrimvar.Get(&uvs);
		for (const auto &uv : uvs) {
			mesh.tex_coords.push_back(vec2(uv[0], uv[1]));
		}
		if (!mesh.has_texture) {
			//TODO: is this required?
			mesh.tex_coords.push_back(vec2(0, 0));
		}

		// load crease data
		uint32_t off = 0;
		VtIntArray usd_crease_indices;
		VtIntArray usd_crease_lengths;
		VtFloatArray usd_crease_sharpness;
		usd_mesh.GetCreaseIndicesAttr().Get(&usd_crease_indices);
		usd_mesh.GetCreaseLengthsAttr().Get(&usd_crease_lengths);
		usd_mesh.GetCreaseSharpnessesAttr().Get(&usd_crease_sharpness);
		for (uint32_t i = 0; i < usd_crease_lengths.size(); ++i) {
			for (uint32_t j = 0; j < usd_crease_lengths[i]-1; ++j) {
				mesh.creases.add(
					usd_crease_indices[off+j],
					usd_crease_indices[off+j+1],
					std::min(usd_crease_sharpness[i],1.f)
				);
			}
			off += usd_crease_lengths[i];
		}

		// load control mesh faces (ctrl_faces)
		VtArray<int> usd_indices;
		usd_mesh.GetFaceVertexIndicesAttr().Get(&usd_indices);
		VtArray<int> usd_face_counts;
		usd_mesh.GetFaceVertexCountsAttr().Get(&usd_face_counts);
		off = 0;
		for (uint32_t i = 0; i < usd_face_counts.size(); ++i) {
			face face;

			if (usd_face_counts[i] < 3) continue;
			for (uint32_t j = 0; j < usd_face_counts[i]; ++j) {
				vertex_config v;
				v.pos = usd_indices[off+j];
				v.tc = off+j;
				face.verts.push_back(v);
			}
			mesh.faces.push_back(face);
			off += usd_face_counts[i];
		}

		mesh.update();
	}

	// Export object to obj format
	//
	// Parameters:
	// outfile_path:	Output path of obj file
	// write_normals:	If true, exports two more obj files
	// 					containing face and vertex normals
	// mtllib_path:		Path to the mtl file, empty string if not provided.
	// 					The path is relative to the outfile_path.
	// 					The mtl file is not exported and needs to be put
	// 					in place manually to use the obj export correctly.
	//
	// Example usage:
	// object o;
	//
	// o.write_obj("output/subd/out_test_subd.obj", true, "cornell_sibenik.mtl");
	//
	void object::write_obj(std::string outfile_path, bool write_normals, std::string mtllib_path, float n_len) {
		ofstream outfile;
		outfile.open(outfile_path);
		if (mtllib_path != "")
			outfile << "mtllib " << mtllib_path << endl << endl;

		outfile << "o " << name << endl;

		// Write vertices
		for (uint i = 0; i < mesh.vertices.size(); i++) {
			const vec3 &v = mesh.vertices[i].pos;
			outfile << "v " << v.x << " " << v.y << " " << v.z << endl;
		}

		// Write normals
		if (mesh.has_normals) {
			outfile << endl;
			for (uint i = 0; i < mesh.normals.size(); i++) {
				const vec3 &n = mesh.normals[i];
				outfile << "vn " << n.x << " " << n.y << " " << n.z << endl;
			}
		}

		// Write texture coordinates
		if (mesh.has_texture) {
			outfile << endl;
			for (uint i = 0; i < mesh.tex_coords.size(); i++) {
				const vec2 &tc = mesh.tex_coords[i];
				outfile << "vt " << tc.x << " " << tc.y << endl;
			}
		}

		// Write faces
		outfile << endl;
		if (has_material())
			outfile << "usemtl " << material << endl;

		for (uint i = 0; i < mesh.faces.size(); i++) {
			const face &f = mesh.faces[i];
			outfile << "f";
			for (uint j = 0; j < f.verts.size(); j++) {
				const vertex_config &v_cfg = f.verts[j];
				int tc_index = v_cfg.tc+1;
				int pos_index = v_cfg.pos+1;
				int normal_index = i+1;
				if (mesh.has_normals && mesh.has_texture)
					outfile << " " << pos_index  << "/" << tc_index << "/" << normal_index;
				else if (mesh.has_normals)
					outfile << " " << pos_index << "//" << normal_index;
				else if (mesh.has_texture)
					outfile << " " << pos_index  << "/" << tc_index;
				else
					outfile << " " << pos_index;
			}

			outfile << endl;
		}

		outfile.close();

		if (write_normals) {
			std::string path_without_ext = outfile_path.substr(0, outfile_path.find_last_of('.'));

			outfile.open(path_without_ext + "_normals_verts.obj");
			outfile << "o " << name << "_normals_verts" << endl;

			int i = 1;
			for (auto &v : mesh.vertices) {
				outfile << "v " << v.pos.x << " " << v.pos.y << " " << v.pos.z << endl;
				outfile << "v " << v.pos.x+n_len*v.norm.x << " " << v.pos.y+n_len*v.norm.y << " " << v.pos.z+n_len*v.norm.z << endl;
				outfile << "l " << i << " " << i+1 << endl;
				i += 2;
			}

			outfile.close();
			
			outfile.open(path_without_ext + "_normals_faces.obj");
			outfile << "o " << name << "_normals_faces" << endl;

			int v_count = 1;
			for (int i = 0; i < mesh.faces.size(); i++) {
				face &face = mesh.faces[i];
				vec3 &norm = mesh.normals[i];

				vec3 f_center(0);
				for (int j = 0; j < face.size(); j++) {
					f_center += mesh.vertices[face.verts[j].pos].pos;
				}
				f_center *= 1.f/face.size();

				outfile << "v " << f_center.x << " " << f_center.y << " " << f_center.z << endl;
				outfile << "v " << f_center.x+n_len*norm.x << " " << f_center.y+n_len*norm.y << " " << f_center.z+n_len*norm.z << endl;
				outfile << "l " << v_count << " " << v_count+1 << endl;
				v_count += 2;
			}

			outfile.close();
		}
	}


	/* mesh implementation */

	void mesh::update() {
		time_this_block(mesh_update);
		// Calculate adjacent edges and faces
		edges.clear();
		for (uint i = 0; i < faces.size(); i++) {
			face& f = faces[i];
			for (uint j = 0; j < f.size(); j++)
				add_edge(f.verts[j].pos, f.verts[(j+1)%f.size()].pos, i);
		}

		calculate_face_normals();
		calculate_vertex_normals();

		edges.finish_init();
	}

	void mesh::subdivide(uint32_t level) {
		// Nothing to do on subdivision level 0
		assert(level >= 0);
		if (level == 0) {
			return;
		}

		// TODO: handling for extraordinary faces/nodes (harder to predict exact size)
		// reserve space for every subd patch
		if (storage_type_patches) {
			patches.reserve(faces.size());

			for (uint32_t i = 0; i < faces.size(); ++i) {
				face &face = faces[i];
				if (face.size() == 4) {
					// Initialize patches for quad base faces.
					// Other n-gon base faces need to be treated
					// differently in the subdivision process.
					faces[i].patch_id = patches.size();
					patches.emplace_back(level);
				}
			}
		}

		for (uint32_t l = 1; l <= level; ++l) {
			subdivide_internal(level);
			if (subd_debug && storage_type_patches) {
				for (auto patch : patches) {
					patch.print_verts();
					patch.print_vert_tcs();
				}
			}
		}

		calculate_vertex_normals();
	}

	void mesh::subdivide_internal(uint32_t end_level) {
		mesh new_mesh;

		// Gather face vertices and edges and update vertex information
		vector<vec3> face_vertices;
		int32_t f1 = 0;
		{
			time_this_block(calc_face_verts);
			for (uint i = 0; i < faces.size(); i++) {
				face& f = faces[i];
				vec3 f_new(0);
				for (uint j = 0; j < f.size(); j++)
					f_new += vertices[f.verts[j].pos].pos;

				f_new = 1.f/f.size() * f_new;
				face_vertices.push_back(f_new);
				if (subd_debug)
					cout << "f_new: (" << f_new.x << ", " << f_new.y << ", " << f_new.z << ")" << endl;

				f1 += f.size();
			}
		}

		{
			time_this_block(assign_edge_creases);
			// Assign edge crease sharpness
			auto crease_it = creases.iterator();
			for (int i = 0; i < creases.size(); i++) {
				edge &c = creases.get_next(crease_it);
				edge &e = edges.get(c.v1, c.v2);
				e.sharpness = c.sharpness;
			}
		}

		vector<vec3> edge_vertices;
		{
			time_this_block(calc_cur_edge_verts);
			// Calculate current edge vertices
			auto edge_it = edges.iterator();
			for (int i = 0; i < edges.size(); i++) {
				edge &e = edges.get_next(edge_it);
				edge_vertices.push_back(1.f/2 * (vertices[e.v1].pos+vertices[e.v2].pos));

				if (subd_debug) {
					cout << "edge: (" << e.v1 << "," << e.v2 << ") f:";
					for (int j : e.face_ids) {
						cout << " " << j;
					}
					cout << endl;
				}
			}
		}

		vector<vec3> e_news;
		{
			time_this_block(calc_edge_verts);
			// Calculate new edge vertices
			auto edge_it = edges.iterator();
			for (int i = 0; i < edges.size(); i++) {
				edge &e = edges.get_next(edge_it);
				vec3 e_new;

				if (e.sharpness <= 0) {
					// Smooth edge
					e_new = calc_smooth_edge_vertex(e, face_vertices);
				}
				else if(e.sharpness >= 1.f) {
					// Infinitely Sharp edge
					e_new = edge_vertices[i];
				}
				else {
					// Semi-sharp edge
					e_new = e.sharpness * edge_vertices[i] + (1-e.sharpness) * calc_smooth_edge_vertex(e, face_vertices);
				}

				e_news.push_back(e_new);

				if (subd_debug)
					cout << "e_new: (" << e_new.x << ", " << e_new.y << ", " << e_new.z << ")" << endl;
			}
		}

		vector<vec3> v_news;
		{
			time_this_block(calc_vertex_verts);
			// Calculate new vertex points
			for (uint i = 0; i < vertices.size(); i++) {
				ctrl_vertex &v = vertices[i];
				vector<edge *> sharp_edges;
				vector<int> sharp_edge_ids;
				float v_sharpness = 0;
				vec3 v_new;

				for (uint j = 0; j < v.edge_ids.size(); j++) {
					edge &v_edge = edges.get(v.edge_ids[j]);
					if (v_edge.sharpness > 0) {
						sharp_edges.push_back(&v_edge);
						sharp_edge_ids.push_back(j);
						v_sharpness += v_edge.sharpness;
					}
				}
				if (sharp_edge_ids.size() > 0)
					v_sharpness = 1.f/sharp_edge_ids.size() * v_sharpness;
				
				uint n_sharp_edges = sharp_edges.size();
				vec3 v_smooth = calc_vertex_vertex(v, edge_vertices, face_vertices);
				if (n_sharp_edges <= 1) {
					// zero or one adjacent sharp edges -> smooth vertex rule
					v_new = v_smooth;
				}
				else if (n_sharp_edges == 2) {
					// two adjacent sharp edges -> crease rule (or blend between crease vertex and corner mask)
					vec3 e1 = calc_sharp_edge_vertex(*sharp_edges[0]);
					vec3 e2 = calc_sharp_edge_vertex(*sharp_edges[1]);
					// TODO: which weight to use? (compare literature, blender, etc.)
					//v_new = 1.f/8 * (6.f * v.v + e1 + e2);
					v_new = 1.f/4 * (2.f * v.pos + e1 + e2);
				}
				else {
					// three or more adjacent sharp edges -> corner rule
					v_new = v.pos;
				}

				if (n_sharp_edges > 1) {
					v_new = v_sharpness * v_new + (1 - v_sharpness) * v_smooth;
				}

				v_news.push_back(v_new);
				
				if (subd_debug)
					cout << "v_new: (" << v_new.x << ", " << v_new.y << ", " << v_new.z << ")" << endl;
			}
		}

		uint old_vertices = vertices.size();
		int off_vert = 0;
		int off_edge = off_vert + vertices.size();
		int off_face = off_edge + edges.size();

		vertices.clear();

		{
			time_this_block(assign_verts);
			// Assign vertices
			{
				for (uint i = 0; i < old_vertices; i++) {
					vertices.push_back(v_news[i]);
				}
				for (vec3 &e : e_news) {
					vertices.push_back(e);
				}
				for (vec3 &f : face_vertices) {
					vertices.push_back(f);
				}
			}
		}

		{
			time_this_block(assign_faces);
			// Assign faces
			normals.clear();
			for (uint i = 0; i < faces.size(); i++) {
				face &f = faces[i];
				int n = f.size();
				int off_tcs = new_mesh.tex_coords.size();

				subd_patch *patch_opt = f.patch_id >= 0 ? &patches[f.patch_id] : nullptr;

				int step = 2;
				uint32_t quad_id = patch_opt ? (patch_opt->len() * f.patch_y + f.patch_x) * step : 0;
				
				// Calculate texture coordinates of the face
				if (has_texture) {
					vec2 tc_face(0);
					for (int j = 0; j < n; j++) {
						vec2 current_tc = tex_coords[f.verts[j].tc];
						vec2 next_tc = tex_coords[f.verts[(j+1)%n].tc];
						tc_face += current_tc;

						new_mesh.tex_coords.push_back(current_tc);						// vertex tc
						new_mesh.tex_coords.push_back(.5f * (current_tc + next_tc));	// edge tc
					}
					tc_face /= n; // face tc
					new_mesh.tex_coords.push_back(tc_face);
				}

				if (storage_type_patches) {
					if (patch_opt)
						patch_opt->material_id = f.material_id;
					else if (n != 4) {
						// Extraordinary faces do not fit in the patch structure.
						// After the first subdivision these faces are split into n quads, which match the patch structure.
						// Because of that, n patches are added for every extraordinary face.
						assert(end_level >= 1);
						uint32_t patch_level = end_level-1;
						patches.resize(patches.size() + n, subd_patch(patch_level, f.material_id));
					}
				}

				for (int j = 0; j < n; j++) {
					face new_f;
					new_f.material_id = f.material_id;

					int edge_vert1_id = f.verts[(j+1)%n].pos;
					int edge_vert2_id = f.verts[((j-1)%n+n)%n].pos;
					int vert_ids[4]; // [0] v_vert, [1] e_vert1, [2] f_vert, [3] e_vert2
					vert_ids[0] = off_vert+f.verts[j].pos;
					vert_ids[1] = off_edge+edges.get_index(f.verts[j].pos, edge_vert1_id);
					vert_ids[2] = off_face+i;
					vert_ids[3] = off_edge+edges.get_index(f.verts[j].pos, edge_vert2_id);
					int tex_ids[4];
					tex_ids[0] = off_tcs + j*2;				// vertex tc
					tex_ids[1] = off_tcs + j*2+1;			// edge tc
					tex_ids[2] = off_tcs + n*2;				// face tc
					tex_ids[3] = off_tcs + (j-1+n)%n * 2+1;	// edge tc

					for (int k = 0; k < 4; k++)
						new_f.verts.push_back(vertex_config());

					// ----- subd grid -----

					if (subd_debug) {
						std::cout << "V : " << vertices[vert_ids[0]].pos << std::endl;
						std::cout << "E1: " << vertices[vert_ids[1]].pos << std::endl;
						std::cout << "F : " << vertices[vert_ids[2]].pos << std::endl;
						std::cout << "E2: " << vertices[vert_ids[3]].pos << std::endl;
					}

					uint32_t v_id[4];
					v_id[0] = ((4-j + 0)%4+4)%4;
					v_id[1] = ((4-j + 1)%4+4)%4;
					v_id[2] = ((4-j - 1)%4+4)%4;
					v_id[3] = ((4-j - 2)%4+4)%4;

					if (storage_type_patches) {
						// Init patches for irregular faces
						int irregular_patch_id = -1;
						if (n != 4) {
							irregular_patch_id = patches.size() - n + j;
							patch_opt = &patches[irregular_patch_id];
						}

						assert(patch_opt != nullptr);
						subd_patch &patch = *patch_opt;

						uint32_t start_id = 0;
						if (n == 4) {
							if (j == 1) start_id = patch.vert_right(start_id);
							if (j == 3) start_id = patch.vert_down(start_id);
							if (j == 2) start_id = patch.vert_down_right(start_id);
						}

						new_f.patch_id = irregular_patch_id < 0 ? f.patch_id : irregular_patch_id;
						if (n == 4) {
							uint32_t base_x = f.patch_x * 2;
							uint32_t base_y = f.patch_y * 2;
							if (j == 0) {
								new_f.patch_x = base_x + 0;
								new_f.patch_y = base_y + 0;
							}
							else if (j == 1) {
								new_f.patch_x = base_x + 1;
								new_f.patch_y = base_y + 0;
							}
							else if (j == 2) {
								new_f.patch_x = base_x + 1;
								new_f.patch_y = base_y + 1;
							}
							else if (j == 3) {
								new_f.patch_x = base_x + 0;
								new_f.patch_y = base_y + 1;
							}
						}
						else {
							new_f.patch_x = 0;
							new_f.patch_y = 0;
						}

						uint32_t final_id = patch.len() * new_f.patch_y + new_f.patch_x;
						ctrl_vertex *vert = &vertices[vert_ids[v_id[0]]];
						vert->patch_positions.push_back({new_f.patch_id, final_id});
						patch.verts[final_id] = *vert;
						if (has_texture)
							patch.verts[final_id].tc = new_mesh.tex_coords[tex_ids[v_id[0]]];

						if (subd_debug) {
							cout << "Write ( " << final_id%patch.len() << " | " << final_id/patch.len() << " ): " << vertices[vert_ids[v_id[0]]].pos << endl;
							cout << "Write TC ( " << final_id%patch.len() << " | " << final_id/patch.len() << " ): " << patch.verts[final_id].tc << endl;
						}
						
						uint32_t tmp_id;
						if (j == 1 || j == 2 || n != 4) {
							tmp_id = quad_id+patch.vert_right(start_id);
							vert = &vertices[vert_ids[v_id[1]]];
							vert->patch_positions.push_back({new_f.patch_id, tmp_id});
							patch.verts[tmp_id] = *vert;
							if (has_texture)
								patch.verts[tmp_id].tc = new_mesh.tex_coords[tex_ids[v_id[1]]];

							if (subd_debug) {
								cout << "Write ( " << tmp_id%patch.len() << " | " << tmp_id/patch.len() << " ): " << vertices[vert_ids[v_id[1]]].pos << endl;
								cout << "Write TC ( " << tmp_id%patch.len() << " | " << tmp_id/patch.len() << " ): " << patch.verts[tmp_id].tc << endl;
							}
						}
						if (j == 2 || j == 3 || n != 4) {
							tmp_id = quad_id+patch.vert_down(start_id);
							vert = &vertices[vert_ids[v_id[2]]];
							vert->patch_positions.push_back({new_f.patch_id, tmp_id});
							patch.verts[tmp_id] = *vert;
							if (has_texture)
								patch.verts[tmp_id].tc = new_mesh.tex_coords[tex_ids[v_id[2]]];

							if (subd_debug)	 {
								cout << "Write ( " << tmp_id%patch.len() << " | " << tmp_id/patch.len() << " ): " << vertices[vert_ids[v_id[2]]].pos << endl;
								cout << "Write TC ( " << tmp_id%patch.len() << " | " << tmp_id/patch.len() << " ): " << patch.verts[tmp_id].tc << endl;
							}
						}
						if (j == 2 || n != 4) {
							tmp_id = quad_id+patch.vert_down_right(start_id);
							vert = &vertices[vert_ids[v_id[3]]];
							vert->patch_positions.push_back({new_f.patch_id, tmp_id});
							patch.verts[tmp_id] = *vert;
							if (has_texture)
								patch.verts[tmp_id].tc = new_mesh.tex_coords[tex_ids[v_id[3]]];
							
							if (subd_debug) {
								cout << "Write ( " << tmp_id%patch.len() << " | " << tmp_id/patch.len() << " ): " << vertices[vert_ids[v_id[3]]].pos << endl;
								cout << "Write TC ( " << tmp_id%patch.len() << " | " << tmp_id/patch.len() << " ): " << patch.verts[tmp_id].tc << endl;
							}
						}
					}

					new_f.verts[0].pos = vert_ids[v_id[0]];
					new_f.verts[1].pos = vert_ids[v_id[1]];
					new_f.verts[2].pos = vert_ids[v_id[3]];
					new_f.verts[3].pos = vert_ids[v_id[2]];

					if (subd_debug) {
						std::cout << "V0 : " << vertices[new_f.verts[0].pos].pos << std::endl;
						std::cout << "V1: " << vertices[new_f.verts[1].pos].pos << std::endl;
						std::cout << "V2 : " << vertices[new_f.verts[2].pos].pos << std::endl;
						std::cout << "V3: " << vertices[new_f.verts[3].pos].pos << std::endl;
					}

					new_f.verts[0].tc = tex_ids[v_id[0]];
					new_f.verts[1].tc = tex_ids[v_id[1]];
					new_f.verts[2].tc = tex_ids[v_id[3]];
					new_f.verts[3].tc = tex_ids[v_id[2]];

					new_mesh.faces.push_back(new_f);

					// ----- ---- ---- -----

					if (!new_mesh.creases.exists(f.verts[j].pos, edge_vert1_id)) {
						float s = 0.f;

						ctrl_vertex &v = vertices[vert_ids[0]];
						uint64_t e_id = edges.get_key(f.verts[j].pos, edge_vert1_id);
						edge &e = edges.get(e_id);
						float max_adjacent_sharpness = 0.f;
						for (uint k = 0; k < v.edge_ids.size(); k++) {
							if (v.edge_ids[k] != e_id) {
								edge &adj_edge = edges.get(v.edge_ids[k]);
								if (adj_edge.sharpness > max_adjacent_sharpness)
									max_adjacent_sharpness = adj_edge.sharpness;

							}
						}

						// TODO: only consider edges with sharpness?
						//s = 1.f/4 * (3*e.sharpness + max_adjacent_sharpness);
						s = e.sharpness;

						new_mesh.creases.add(vert_ids[0], vert_ids[1], s);
					}
					// TODO: is this second case neccessary or is it guaranteed that the first case covers all new edges?
					if (!new_mesh.creases.exists(f.verts[j].pos, edge_vert2_id)) {
						// ...
					}
					
					glm::vec3 u = vertices[new_f.verts[0].pos].pos - vertices[new_f.verts[1].pos].pos;
					glm::vec3 v = vertices[new_f.verts[2].pos].pos - vertices[new_f.verts[1].pos].pos;
					glm::vec3 normal = glm::normalize(glm::cross(v, u));
					normals.push_back(normal);
				}
			}
		}

		{
			time_this_block(cleanup);

			// Propagate new crease values
			creases.clear();
			auto crease_it = new_mesh.creases.iterator();
			for (int i = 0; i < new_mesh.creases.size(); i++) {
				edge &e = new_mesh.creases.get_next(crease_it);
				if (e.sharpness > 0)
					creases.add(e.v1, e.v2, e.sharpness);
			}

			tex_coords.clear();
			for (vec2 tc : new_mesh.tex_coords)
				tex_coords.push_back(tc);

			faces.clear();
			for (face &f : new_mesh.faces) {
				faces.push_back(f);
			}
		}

		{
			// Calculate adjacent edges and faces
			update_topology();

			// Assign single tc to vertices (used for displacement)
			pass_tcs();

			// Normals have been calculated in this method
			has_normals = true;
		}
	}

	void mesh::update_topology() {
		//time_this_block(topology);

		// Pass 1: Calculate edges

		// Reset topology
		edges.clear();

		// 1. Edge collection (Map, parallel)
		vector<vector<edge>> buckets(faces.size());
		{
			time_this_block(topology_edge_collect);

			#pragma omp parallel for
			for (uint i = 0; i < faces.size(); i++) {
				face& f = faces[i];
				auto &bucket = buckets[i];
				for (uint j = 0; j < f.size(); j++) {
					int a = f.verts[j].pos;
					int b = f.verts[(j+1)%f.size()].pos;
					bucket.emplace_back(a<=b ? a:b, a<=b ? b:a, 0.f, i);
				}
			}
		}

		// 2. Flatten/Group edge buckets (serial)
		vector<edge> merged;
		{
			time_this_block(topology_merge);

			uint32_t merged_size = 0;

			for (const auto &bucket : buckets)
				merged_size += bucket.size();

			merged.reserve(merged_size);
			for (const auto &bucket : buckets)
				merged.insert(merged.end(), bucket.begin(), bucket.end());
		}

		// 3. Shuffle/Sort edges by key (possibly parallel)
		/*{
			time_this_block(topology_sort);
			std::sort(merged.begin(), merged.end(), [](const edge &a, const edge &b) {
				uint64_t val_a = ((uint64_t)a.v1 << 32) | a.v2;
				uint64_t val_b = ((uint64_t)b.v1 << 32) | b.v2;
				return val_a < val_b;
			});
		}

		// 4. Reduce edges (serial, possibly parallel?)
		{
			time_this_block(topology_reduce);
			
			auto it = merged.begin();
			while (it != merged.end()) {
				//edge *e = &(*it);
				edge &e = *it;
				edge reduced_edge(e.v1, e.v2);

				while (it != merged.end() && it->v1 == reduced_edge.v1 && it->v2 == reduced_edge.v2) {
					reduced_edge.face_ids.push_back(it->face_ids[0]);
					++it;
				}
				edges.add(reduced_edge);
			}

			edges.finish_init();
		}*/

		{
			time_this_block(topology_reduce_compact);

			for (int i = 0; i < merged.size(); i++) {
				const edge &e = merged[i];
				uint64_t e_id = edges.get_key(e.v1, e.v2);
				if (e_id == ((uint64_t)-1))
					e_id = edges.add(e.v1, e.v2);

				 edges.get(e_id).face_ids.push_back(e.face_ids[0]);
			}

			edges.finish_init();
		}


		// Pass 2: Update vertices

		{
			time_this_block(topology_vertices);

			for (uint i = 0; i < faces.size(); i++) {
				face& f = faces[i];
				for (uint j = 0; j < f.size(); j++) {
					uint64_t a = f.verts[j].pos;
					uint64_t b = f.verts[(j+1)%f.size()].pos;
					uint64_t e_val = ((a<=b ? a:b) << 32) | (a<=b ? b:a);
					update_vertex(vertices[a], i, e_val);
					update_vertex(vertices[b], i, e_val);
				}
			}
		}
	}

	uint64_t mesh::add_edge(int a, int b, int f_id) {
		uint64_t e_id = edges.get_key(a, b);
		if (e_id == ((uint64_t)-1))
			e_id = edges.add(a, b);
		
		edge &e = edges.get(e_id);

		update_vertex(vertices[a], f_id, e_id);
		update_vertex(vertices[b], f_id, e_id);

		vector<int> &face_ids = e.face_ids;
		if (!e.face_exists(f_id)) {
			face_ids.push_back(f_id);
		}

		return e_id;
	}

	void mesh::update_vertex(ctrl_vertex &v, int f_id, uint64_t e_id) {
		if (!v.edge_exists(e_id))
			v.edge_ids.push_back(e_id);

		if (!v.face_exists(f_id))
			v.face_ids.push_back(f_id);
	}

	// Calculate smooth edge vertex
	vec3 mesh::calc_smooth_edge_vertex(const edge &e, const vector<vec3> &face_vertices) {
		int n_faces = e.face_ids.size();
		if (n_faces == 2)
			return 1.f/4 * (vertices[e.v1].pos + vertices[e.v2].pos + face_vertices[e.face_ids[0]] + face_vertices[e.face_ids[1]]);
		else if (n_faces == 1)
			return calc_sharp_edge_vertex(e);
		else if (n_faces > 2) {
			// TODO: verify if this case is equivalent to literature!
			// Mine: implicit handling of an edge with n_faces > 2
			vec3 e_new = vertices[e.v1].pos + vertices[e.v2].pos;
			for (int j = 0; j < n_faces; j++)
				e_new += face_vertices[e.face_ids[j]];

			return e_new / (float)(2+n_faces);
		}
		else
			cout << "Error: unhandled face number for edge!" << endl;

		return vec3(0);
	}

	// Calculate current / sharp edge vertex
	vec3 mesh::calc_sharp_edge_vertex(const edge &e) {
		return .5f * (vertices[e.v1].pos + vertices[e.v2].pos);
	}

	vec3 mesh::calc_vertex_vertex(const ctrl_vertex &v, const std::vector<vec3> &edge_vertices, const vector<vec3> &face_vertices) {
		uint n = v.edge_ids.size();
		if (n == v.face_ids.size()) {
			vec3 Q(0), R(0);
			for (uint j = 0; j < n; j++)
				Q += face_vertices[v.face_ids[j]];

			Q /= n;

			for (uint j = 0; j < n; j++)
				R += edge_vertices[edges.get_index(v.edge_ids[j])];

			R /= n;

			return 1.f/n * (Q + 2.f*R + (n-3.f)*v.pos);
		}
		else {
			int relevant_edges = 0;
			vec3 R(0);
			for (uint j = 0; j < v.edge_ids.size(); j++) {
				// check if edge is at a hole
				edge &e = edges.get(v.edge_ids[j]);
				if (e.face_ids.size() != 1)
					continue;

				relevant_edges++;

				R += edge_vertices[edges.get_index(v.edge_ids[j])];
			}

			// TODO: verify if this case is equivalent to literature!
			// Mine: double weight the vertex position here
			return 1.f/(relevant_edges+2) * (R + v.pos+v.pos);
		}
	}

	// Get vertex id from position
	int mesh::get_vert_id(vec3 v_pos) {
		for (int i = 0; i < vertices.size(); i++) {
			if (v_pos == vertices[i].pos)
				return i;
		}

		return -1;
	}

	void mesh::calculate_face_normals() {
		for (int i = 0; i < faces.size(); i++) {
			const face &f = faces[i];
			glm::vec3 u = vertices[f.verts[0].pos].pos - vertices[f.verts[1].pos].pos;
			glm::vec3 v = vertices[f.verts[2].pos].pos - vertices[f.verts[1].pos].pos;
			glm::vec3 normal = glm::normalize(glm::cross(v, u));
			normals.push_back(normal);
		}
	}

	// Calculate vertex normals from face normals
	void mesh::calculate_vertex_normals() {
		auto calc_normal = [this](ctrl_vertex &vert) {
			vert.norm = vec3(0);
			for (uint32_t j = 0; j < vert.face_ids.size(); ++j)
				vert.norm += normals[vert.face_ids[j]];

			vert.norm *= 1.f/vert.face_ids.size();
			vert.norm = glm::normalize(vert.norm);
			if (storage_type_patches) {
				for (auto &position : vert.patch_positions)
					patches[position.first].verts[position.second].norm = vert.norm;

			}
			
			has_normals = true;

			if (subd_debug)
				cout << "Normal: " << vert.norm << endl;
		};

		for (uint32_t i = 0; i < vertices.size(); ++i)
			calc_normal(vertices[i]);

	}

	void mesh::pass_tcs() {
		for (auto &f : faces) {
			for (auto &vc : f.verts) {
				auto &v = vertices[vc.pos];
				v.tc = tex_coords[vc.tc];
			}
		}
	}

	// Ear cutting triangulation
	//
	// Reference:
	// https://wiki.delphigl.com/index.php/Ear_Clipping_Triangulierung
	//
	// The implementation is derived from the reference description, but
	// directly uses 3D coords.
	// TODO: the current implementation fails at triangulating non-convex n-gons
	void mesh::triangulate() {
		mesh new_mesh;

		for (uint i = 0; i < faces.size(); i++) {
			face &f = faces[i];
			int n = f.verts.size();
			int j = -1;
 			int j_max = n*n; // upper bound
			while (n > 3) {
				j++;
				if (j > j_max) {
					std::cerr << "Error: Could not triangulate face, skipped: { ("
						<< vertices[f.verts[0].pos].pos << ") ("
						<< vertices[f.verts[1].pos].pos << ") ("
						<< vertices[f.verts[2].pos].pos << ") ("
						<< vertices[f.verts[3].pos].pos << ") }"
						<< std::endl
						<< "Normal: " << normals[i]
						<< std::endl;
						break;
				}
				assert(j <= j_max);
				int i_x = j%n;
				int i_a = ((j-1) % n + n) % n;
				int i_b = (j+1) % n;
				vec3 x = vertices[f.verts[i_x].pos].pos;
				vec3 a = vertices[f.verts[i_a].pos].pos;
				vec3 b = vertices[f.verts[i_b].pos].pos;
				vec3 to_a = a - x;
				vec3 to_b = b - x;

				// test if angle at x is convex inside the polygon, otherwise continue
				float d = glm::determinant(glm::mat3x3(to_b, to_a, normals[i]));
				if (d <= 0)
					continue;

				// test if the triangle x-a-b is an ear (all other points of the polygon are outside this triangle),
				// otherwise continue
				// 1. calc normal (cross(to_b, to_a))
				// 2. calc direction v orthogonal to a_to_b and inside the triangle (cross(normal, b_to_a))
				// 3. test for each other point if it is left or right of b_to_a
				vec3 normal = cross(to_b, to_a);
				vec3 b_to_a = a - b;
				vec3 v = cross(normal, b_to_a);

				int i_k = (i_b+1)%n;
				bool skip_x = false;
				while (i_k != i_a) {
					vec3 b_to_k = vertices[f.verts[i_k].pos].pos - b;
					if (dot(v, b_to_k) >= 0) {
						skip_x = true;
						break;
					}
					i_k = (i_k+1)%n;
				}
				if (skip_x)
					continue;

				// create new face (triangle)
				face new_f;
				new_f.material_id = f.material_id;
				new_f.normal = normals[i];
				new_f.verts.push_back(vertex_config());
				new_f.verts.push_back(vertex_config());
				new_f.verts.push_back(vertex_config());

				// assign vertex points
				new_f.verts[0].pos = f.verts[i_x].pos;
				new_f.verts[1].pos = f.verts[i_b].pos;
				new_f.verts[2].pos = f.verts[i_a].pos;

				// assign texture coordinates
				new_f.verts[0].tc = f.verts[i_x].tc;
				new_f.verts[1].tc = f.verts[i_b].tc;
				new_f.verts[2].tc = f.verts[i_a].tc;

				// works only for planar polygons:
				new_mesh.normals.push_back(normals[i]);
				new_mesh.faces.push_back(new_f);

				f.verts.erase(f.verts.begin() + i_x);
				n = f.verts.size();
			}
			// If triangulation failed for this face, it will be skipped and not added
			if (f.size() == 3) {
				new_mesh.faces.push_back(f);
				new_mesh.normals.push_back(normals[i]);
			}
		}

		normals.clear();
		faces.clear();
		for (uint i = 0; i < new_mesh.faces.size(); i++) {
			faces.push_back(new_mesh.faces[i]);
			normals.push_back(new_mesh.normals[i]);
		}
	}

	void mesh::displace(sample_tex sample, float strength) {
		if (strength == 0.f)
			return;

		for (uint32_t i = 0; i < vertices.size(); i++) {
		//for (auto &f : faces) {
			//for (auto &vc : f.verts) {
				auto &v = vertices[i];
				float displacement = 0.f;
				if (sample) {
					vec4 s = sample(v.tc);
					////vec4 s = sample(tex_coords[v.faces[0].tc]);
					//vec4 s = sample(tex_coords[vc.tc]);
					displacement = strength * 1.f/3 * (s.x + s.y + s.z);
					displacement -= 0.5f * strength;
				}
				else {
					displacement = strength * static_cast<float>(rand() / static_cast<float>(RAND_MAX));
				}
				v.pos += displacement * v.norm;
			//}
		}

		if (storage_type_patches) {
			for (auto &patch : patches) {
				for (auto &v : patch.verts) {
					float displacement = 0.f;
					if (sample) {
						vec4 s = sample(v.tc);
						////vec4 s = sample(tex_coords[v.faces[0].tc]);
						//vec4 s = sample(tex_coords[vc.tc]);
						displacement = strength * 1.f/3 * (s.x + s.y + s.z);
						displacement -= 0.5f * strength;
					}
					else {
						displacement = strength * static_cast<float>(rand() / static_cast<float>(RAND_MAX));
					}
					v.pos += displacement * v.norm;
				}
			}
		}

		update();
		//calculate_vertex_normals();
	}
}