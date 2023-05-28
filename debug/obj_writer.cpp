#include "config.h"
#include "obj_writer.h"
#include <vector>
#include <glm/gtx/color_space.hpp>
#include "libgi/color.h"



namespace obj {

	int material::material_id = 0;

	material::material(const std::string &name) 
	: name(name),
	  Ns(0), Ka(0, 0, 0), Kd(0, 0, 0), Ks(0, 0, 0), Ke(0, 0, 0), Ni(0), d(1), illum(0) {
	}

	material::material(const std::string &name,
	                   float Ns, const glm::vec3 &Ka, const glm::vec3 &Kd, const glm::vec3 &Ks, const glm::vec3 &Ke, float Ni, float d, int illum) 
	: name(name),
	  Ns(Ns), Ka(Ka), Kd(Kd), Ks(Ks), Ke(Ke), Ni(Ni), d(d), illum(illum) {
	}

	material::material(const std::string &name,
	                   float r, float g, float b)
	: material(name, 0, vec3(0), vec3(r, g, b), vec3(0), vec3(0), 0, 0, 0) {
	}

	material::material(float r, float g, float b)
	: material("obj_material_" + std::to_string(material_id++), r, g, b) {
	}

	material::material(const ::material &mtl)
	: name(mtl.name), Ns(0),
	  Ka(0), Kd(0, 0, 0), Ks(0, 0, 0), Ke(mtl.emissive), Ni(0), d(1), illum(0) {
		constexpr const float one_over_2_2 = 1.f / 2.2f;
		const vec3 tmp = glm::pow(mtl.albedo, vec3(one_over_2_2, one_over_2_2, one_over_2_2));
		if (luma(tmp) > 1e-4)
			Kd = tmp;
		else
			Ks = tmp;
	}

	material material::hsv(const std::string &name, float h, float s, float v) {
		assert(h >= 0.0f && h <= 360.f);
		assert(s >= 0.0f && s <= 1.f);
		assert(v >= 0.0f && v <= 1.f);

		vec3 rgb = glm::rgbColor(vec3(h, s, v));
		return material(name, rgb.x, rgb.y, rgb.z);
	}
	
	material material::hsv(float h, float s, float v) {
		return material::hsv("obj_material_" + std::to_string(material_id++), h, s, v);
	}

	void material::write(std::ofstream &out) {
		out << "newmtl" << " " << name  << std::endl;
		out << "Ka"     << " " << Ka.x  << " " << Ka.y << " " << Ka.z << std::endl;
		out << "Kd"     << " " << Kd.x  << " " << Kd.y << " " << Kd.z << std::endl;
		out << "Ks"     << " " << Ks.x  << " " << Ks.y << " " << Ks.z << std::endl;
		out << "Ke"     << " " << Ke.x  << " " << Ke.y << " " << Ke.z << std::endl;
		out << "Ni"     << " " << Ni    << std::endl;
		out << "d"      << " " << d     << std::endl;
		out << "illum"  << " " << illum << std::endl;
	}

	obj_writer::material_lib::material_lib(const std::filesystem::path &path)
	: file_path(path) {
	}

	obj_writer::material_lib::material_lib()
	: file_path("") {
	}

	void obj_writer::material_lib::write() {
		std::ofstream out(file_path);
		for (auto &[_, m] : materials)
			m.write(out);

		out.close();
	}

	void obj_writer::material_lib::add_material(const obj::material &material) {
		if (!materials.count(material.name))
			materials.emplace(material.name, material);
	}

	obj_writer::obj_writer(const std::filesystem::path &file_path)
	: file_path(file_path),
	  out(file_path),
	  vertex_index(0),
	  active_material("") {
		init_default_mtllib();
	}

	void obj_writer::init_default_mtllib() {
		std::filesystem::path default_mtllib_path = file_path; 
		default_mtllib_path.replace_extension("mtl");
		mtl_lib.file_path = default_mtllib_path;
		out << "mtllib" << " " << mtl_lib.file_path << std::endl;
	}

	int obj_writer::vertex(const glm::vec3 &v) {
		out << "v" << " " << v.x << " " << v.y << " " << v.z << std::endl;
		return ++vertex_index;
	}

	int obj_writer::vertex(float x, float y, float z) {
		return vertex(glm::vec3(x, y, z));
	}

	obj_writer& obj_writer::operator<<(const obj::triangle &tri) {
		const int a = vertex(tri.x);
		const int b = vertex(tri.y);
		const int c = vertex(tri.z);

		*this << obj::index_triangle(a, b, c);
		return *this;
	}

	obj_writer& obj_writer::operator<<(const obj::index_triangle &index_tri) {
		out << "f" << " " << index_tri.a << " " << index_tri.b << " " << index_tri.c << std::endl;
		return *this;
	}

	obj_writer& obj_writer::operator<<(const obj::solid_box &sl_box) {
		const vec3 &min = sl_box.min;
		const vec3 &max = sl_box.max;

		const unsigned int left_bottom_front  = vertex(min.x, min.y, min.z);
		const unsigned int left_bottom_back   = vertex(min.x, min.y, max.z);
		const unsigned int left_top_front     = vertex(min.x, max.y, min.z);
		const unsigned int left_top_back      = vertex(min.x, max.y, max.z);
		const unsigned int right_bottom_front = vertex(max.x, min.y, min.z);
		const unsigned int right_bottom_back  = vertex(max.x, min.y, max.z);
		const unsigned int right_top_front    = vertex(max.x, max.y, min.z);
		const unsigned int right_top_back     = vertex(max.x, max.y, max.z);

		out << "f" << " " << left_bottom_front  << " " << left_bottom_back  << " " << left_top_back      << " " << left_top_front     << std::endl;
		out << "f" << " " << right_bottom_front << " " << right_bottom_back << " " << right_top_back     << " " << right_top_front    << std::endl;
		out << "f" << " " << left_top_back      << " " << left_top_front    << " " << right_top_front    << " " << right_top_back     << std::endl;
		out << "f" << " " << left_bottom_back   << " " << left_bottom_front << " " << right_bottom_front << " " << right_bottom_back  << std::endl;
		out << "f" << " " << left_bottom_back   << " " << left_top_back     << " " << right_top_back     << " " << right_bottom_back  << std::endl;
		out << "f" << " " << left_bottom_front  << " " << left_top_front    << " " << right_top_front    << " " << right_bottom_front << std::endl;
		return *this;
	}

	obj_writer& obj_writer::operator<<(const obj::wireframe_box &wf_box) {
		const vec3 &min = wf_box.min;
		const vec3 &max = wf_box.max;

		const unsigned int left_bottom_front  = vertex(min.x, min.y, min.z);
		const unsigned int left_bottom_back   = vertex(min.x, min.y, max.z);
		const unsigned int left_top_front     = vertex(min.x, max.y, min.z);
		const unsigned int left_top_back      = vertex(min.x, max.y, max.z);
		const unsigned int right_bottom_front = vertex(max.x, min.y, min.z);
		const unsigned int right_bottom_back  = vertex(max.x, min.y, max.z);
		const unsigned int right_top_front    = vertex(max.x, max.y, min.z);
		const unsigned int right_top_back     = vertex(max.x, max.y, max.z);
		
		out << "l"   << " " << left_bottom_front  << " " << left_bottom_back   << std::endl;
		out << "l"   << " " << left_bottom_front  << " " << left_top_front     << std::endl;
		out << "l"   << " " << left_bottom_front  << " " << right_bottom_front << std::endl;
		out << "l"   << " " << right_bottom_front << " " << right_bottom_back  << std::endl;
		out << "l"   << " " << right_bottom_front << " " << right_top_front    << std::endl;
		out << "l"   << " " << left_top_back      << " " << left_top_front     << std::endl;
		out << "l"   << " " << left_top_back      << " " << left_bottom_back   << std::endl;
		out << "l"   << " " << left_top_back      << " " << right_top_back     << std::endl;
		out << "l"   << " " << right_top_front    << " " << left_top_front     << std::endl;
		out << "l"   << " " << right_top_front    << " " << right_top_back     << std::endl;
		out << "l"   << " " << right_bottom_back  << " " << right_top_back     << std::endl;
		out << "l"   << " " << right_bottom_back  << " " << left_bottom_back   << std::endl;
		
		return *this;
	}

	obj_writer& obj_writer::operator<<(const obj::path &path) {
		const auto &path_vertices = path.path_vertices;
		icosphere ico(path.scaling);
		
		if (path.highlight_path_vertices && !path_vertices.empty())
			*this << ico.pos(path_vertices[0]);

		for (int i = 1; i < path_vertices.size(); ++i) {
			auto &from = path_vertices[i - 1];
			auto &to   = path_vertices[i];
			*this << obj::line(from, to);
			if (path.highlight_path_vertices)
				*this << ico.pos(to);
		}
		return *this;
	}

	obj_writer& obj_writer::operator<<(const obj::line &l) {
		const unsigned int from_index = vertex(l.from);
		const unsigned int to_index   = vertex(l.to);
		
		out << "l" << " " << from_index << " " << to_index << std::endl;
		return *this;
	}

	obj_writer& obj_writer::operator<<(const obj::icosphere &ico_sphere) {
		const float X = 0.525731112119133606f;
		const float Z = 0.850650808352039932f;
		const float N = 0.f;

		const std::vector<glm::vec3> vertices = {
			{-X, N, Z}, {X, N, Z}, {-X, N, -Z}, {X, N, -Z},
			{N, Z, X}, {N, Z, -X}, {N, -Z, X}, {N, -Z, -X},
			{Z, X, N}, {-Z, X, N}, {Z, -X, N}, {-Z, -X, N}
		};

		const std::vector<glm::ivec3> triangles = {
			{1, 5, 2}, {1, 10, 5}, {10, 6, 5}, {5, 6, 9}, {5, 9, 2},
			{9, 11, 2}, {9, 4, 11}, {6, 4, 9}, {6, 3, 4}, {3, 8, 4},
			{8, 11, 4}, {8, 7, 11}, {8, 12, 7}, {12, 1, 7}, {1, 2, 7},
			{7, 2, 11}, {10, 1, 12}, {10, 12, 3}, {10, 3, 6}, {8, 3, 12}
		};

		const unsigned int start_vertex_index = vertex_index;

		for (const glm::vec3 &v : vertices)
			vertex(v * ico_sphere.scaling + ico_sphere.position);

		for (const glm::ivec3 &triangle : triangles)
			*this << obj::index_triangle(triangle.x + start_vertex_index, triangle.y + start_vertex_index, triangle.z + start_vertex_index);
		return *this;
	}

	obj_writer& obj_writer::operator<<(const obj::object &obj) {
		out << "o" << " " << obj.name << std::endl;
		return *this;
	}

	obj_writer& obj_writer::operator<<(const obj::group &grp) {
		out << "g" << " " << grp.name << std::endl;
		return *this;
	}

	obj_writer& obj_writer::operator<<(const obj::material &mtl) {
		mtl_lib.add_material(mtl);
		if (active_material != mtl.name) {
			active_material = mtl.name;
			out << "usemtl" << " " << mtl.name << std::endl;
		}
		return *this;
	}
	
	void obj_writer::close() {
		out.close();
		mtl_lib.write();
	}

	obj_writer::~obj_writer() {
		close();
	}

	const material red("red",     0, glm::vec3(1, 0, 0),  glm::vec3(1, 0, 0),  glm::vec3(0, 0, 0),  glm::vec3(0, 0, 0), 0, 1, 0);
	const material green("green", 0, glm::vec3(0, 1, 0),  glm::vec3(0, 1, 0),  glm::vec3(0, 0, 0),  glm::vec3(0, 0, 0), 0, 1, 0);
	const material blue("blue",   0, glm::vec3(0, 0, 1),  glm::vec3(0, 0, 1),  glm::vec3(0, 0, 0),  glm::vec3(0, 0, 0), 0, 1, 0);
}
