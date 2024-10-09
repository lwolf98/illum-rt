#include <iostream>
#include <fstream>
#include <map>
#include "subdivision.h"

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

	int edge_list::add(int a, int b, float sharpness) {
		normalize_edge_order(a, b);
		edges.push_back(edge(a, b, sharpness));
		return size()-1;
	}

	int edge_list::add(int a, int b) {
		return add(a, b, 0.f);
	}

	int edge_list::get_id(int a, int b) const {
		normalize_edge_order(a, b);
		for (int i = 0; i < size(); i++) {
			const edge &e = edges[i];
			if (e.v1 == a && e.v2 == b)
				return i;
		}
		
		return -1;
	}

	edge& edge_list::get(int id) {
		return edges[id];
	}

	int edge_list::size() const {
		return edges.size();
	}

	bool edge_list::exists(int a, int b) const {
		return get_id(a, b) != -1;
	}

	void edge_list::clear() {
		edges.clear();
	}


	/* ctrl_vertex implementation */

	bool ctrl_vertex::edge_exists(int id) const {
		for (int i : edge_ids)
			if (i == id)
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

	void object::init_object(aiMesh *mesh_ai) {
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
				if (subd_debug)
					cout << "Input tc " << i << ": " << vec3(tc.x, tc.y, tc.z) << endl;
			}
		}
		else {
			//TODO: is this required?
			mesh.tex_coords.push_back(vec2(0, 0));
		}

		// load crease data
		// TODO!
		//mesh.creases.push_back(...);

		// load control mesh faces (ctrl_faces)
		int vert_count = 0;
		for (uint32_t i = 0; i < mesh_ai->mNumFaces; ++i) {
			const aiFace &f = mesh_ai->mFaces[i];
			face face;

			for (uint32_t j = 0; j < f.mNumIndices; ++j) {
				vertex_config v;
				int assimp_vert_id = f.mIndices[j];
				v.pos = vert_map[assimp_vert_id];
				v.tc = assimp_vert_id;
				face.verts.push_back(v);
			}
			mesh.faces.push_back(face);
		}
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
	// ...
	// o.write_obj("output/subd/out_test_subd.obj", true, "cornell_sibenik.mtl");
	//
	void object::write_obj(std::string outfile_path, bool write_normals, std::string mtllib_path) {
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
				outfile << "v " << v.pos.x+v.norm.x << " " << v.pos.y+v.norm.y << " " << v.pos.z+v.norm.z << endl;
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
				outfile << "v " << f_center.x+norm.x << " " << f_center.y+norm.y << " " << f_center.z+norm.z << endl;
				outfile << "l " << v_count << " " << v_count+1 << endl;
				v_count += 2;
			}

			outfile.close();
		}
	}


	/* mesh implementation */

	void mesh::subdivide() {
		mesh new_mesh;

		// Gather face vertices and edges and update vertex information
		edge_list edges;
		vector<vec3> face_vertices;
		for (uint i = 0; i < faces.size(); i++) {
			face& f = faces[i];
			vec3 f_new(0);
			for (uint j = 0; j < f.verts.size(); j++)
				f_new += vertices[f.verts[j].pos].pos;

			f_new = 1.f/f.size() * f_new;
			face_vertices.push_back(f_new);
			if (subd_debug)
				cout << "f_new: (" << f_new.x << ", " << f_new.y << ", " << f_new.z << ")" << endl;

			for (uint j = 0; j < f.size(); j++)
				add_edge(edges, f.verts[j].pos, f.verts[(j+1)%f.size()].pos, i);

		}

		// Assign edge crease sharpness
		for (int i = 0; i < creases.size(); i++) {
			edge &c = creases.get(i);
			edge &e = edges.get(edges.get_id(c.v1, c.v2));
			e.sharpness = c.sharpness;
		}

		// Calculate current edge vertices
		vector<vec3> edge_vertices;
		for (int i = 0; i < edges.size(); i++) {
			edge &e = edges.get(i);
			edge_vertices.push_back(1.f/2 * (vertices[e.v1].pos+vertices[e.v2].pos));

			if (subd_debug) {
				cout << "edge: (" << e.v1 << "," << e.v2 << ") f:";
				for (int j : e.face_ids) {
					cout << " " << j;
				}
				cout << endl;
			}
		}

		// Calculate new edge vertices
		vector<vec3> e_news;
		for (int i = 0; i < edges.size(); i++) {
			edge &e = edges.get(i);
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

		// Calculate new vertex points
		vector<vec3> v_news;
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
			vec3 v_smooth = calc_vertex_vertex(v, edges, edge_vertices, face_vertices);
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

		uint old_vertices = vertices.size();
		int off_vert = 0;
		int off_edge = off_vert + vertices.size();
		int off_face = off_edge + edges.size();

		vertices.clear();

		// Assign vertices
		for (uint i = 0; i < old_vertices; i++) {
			vertices.push_back(v_news[i]);
		}
		for (vec3 &e : e_news) {
			vertices.push_back(e);
		}
		for (vec3 &f : face_vertices) {
			vertices.push_back(f);
		}

		// Assign faces
		normals.clear();
		for (uint i = 0; i < faces.size(); i++) {
			face &f = faces[i];
			int off_tcs = new_mesh.tex_coords.size();
			
			int n = f.size();
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

			for (int j = 0; j < n; j++) {
				face new_f;
				int edge_vert1_id = f.verts[(j+1)%n].pos;
				int edge_vert2_id = f.verts[((j-1)%n+n)%n].pos;
				int v_vert_id = off_vert+f.verts[j].pos;
				int e_vert1_id = off_edge+edges.get_id(f.verts[j].pos, edge_vert1_id);
				int f_vert_id = off_face+i;
				int e_vert2_id = off_edge+edges.get_id(f.verts[j].pos, edge_vert2_id);
				for (int k = 0; k < 4; k++)
					new_f.verts.push_back(vertex_config());

				new_f.verts[0].pos = v_vert_id;		// 1 vertex vertex,	e(4,1) -> calc. sharpness
				new_f.verts[1].pos = e_vert1_id;	// 2 edge vertex,	e(1,2) -> calc. sharpness
				new_f.verts[2].pos = f_vert_id;		// 3 face vertex,	e(2,3) -> smooth edge
				new_f.verts[3].pos = e_vert2_id;	// 4 edge vertex,	e(3,4) -> smooth edge

				new_f.verts[0].tc = off_tcs + j*2;				// vertex tc
				new_f.verts[1].tc = off_tcs + j*2+1;			// edge tc
				new_f.verts[2].tc = off_tcs + n*2;				// face tc
				new_f.verts[3].tc = off_tcs + (j-1+n)%n * 2+1;	// edge tc
				
				new_mesh.faces.push_back(new_f);

				if (!new_mesh.creases.exists(f.verts[j].pos, edge_vert1_id)) {
					float s = 0.f;

					ctrl_vertex &v = vertices[v_vert_id];
					int e_id = edges.get_id(f.verts[j].pos, edge_vert1_id);
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

					new_mesh.creases.add(v_vert_id, e_vert1_id, s);
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

		// Propagate new crease values
		creases.clear();
		for (int i = 0; i < new_mesh.creases.size(); i++) {
			edge &e = new_mesh.creases.get(i);
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

		// Calculate adjacent edges and faces
		edges.clear();
		for (uint i = 0; i < faces.size(); i++) {
			face& f = faces[i];
			for (uint j = 0; j < f.size(); j++)
				add_edge(edges, f.verts[j].pos, f.verts[(j+1)%f.size()].pos, i);
		}

		// Normals have been calculated in this method
		has_normals = true;
	}

	int mesh::add_edge(edge_list &edges, int a, int b, int f_id) {
		int e_id = edges.get_id(a, b);
		if (e_id == -1) {
			e_id = edges.add(a, b);
		}
		edge &e = edges.get(e_id);

		update_vertex(vertices[a], f_id, e_id);
		update_vertex(vertices[b], f_id, e_id);

		vector<int> &face_ids = e.face_ids;
		if (!e.face_exists(f_id)) {
			face_ids.push_back(f_id);
		}

		return e_id;
	}

	void mesh::update_vertex(ctrl_vertex &v, int f_id, int e_id) {
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
			//return edge_vertices[i];
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

	vec3 mesh::calc_vertex_vertex(const ctrl_vertex &v, edge_list &edges, const vector<vec3> &edge_vertices, const vector<vec3> &face_vertices) {
		uint n = v.edge_ids.size();
		if (n == v.face_ids.size()) {
			vec3 Q(0), R(0);
			for (uint j = 0; j < n; j++)
				Q += face_vertices[v.face_ids[j]];

			Q /= n;

			for (uint j = 0; j < n; j++)
				R += edge_vertices[v.edge_ids[j]];

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
				R += edge_vertices[v.edge_ids[j]];
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

	// Calculate vertex normals from face normals
	void mesh::calculate_vertex_normals() {
		for (uint32_t i = 0; i < vertices.size(); ++i) {
			ctrl_vertex &vert = vertices[i];
			vert.norm = vec3(0);
			for (uint32_t j = 0; j < vert.face_ids.size(); ++j)
				vert.norm += normals[vert.face_ids[j]];

			vert.norm *= 1.f/vert.face_ids.size();
			vert.norm = glm::normalize(vert.norm);
			
			has_normals = true;

			if (subd_debug)
				cout << "Normal " << i << ": " << vert.norm << endl;
		}
	}

	// Ear cutting triangulation
	//
	// Reference:
	// https://wiki.delphigl.com/index.php/Ear_Clipping_Triangulierung
	void mesh::triangulate() {
		mesh new_mesh;

		for (uint i = 0; i < faces.size(); i++) {
			face &f = faces[i];
			int n = f.verts.size();
			int j = -1;
			while (n > 3) {
				j++;
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
			new_mesh.faces.push_back(f);
			new_mesh.normals.push_back(normals[i]);
		}

		normals.clear();
		faces.clear();
		for (uint i = 0; i < new_mesh.faces.size(); i++) {
			faces.push_back(new_mesh.faces[i]);
			normals.push_back(new_mesh.normals[i]);
		}
	}
}