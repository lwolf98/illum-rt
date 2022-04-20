#include "base.h"
#include "rni.h"
#include "tracers.h"

#include <iostream>

#define error(x) { std::cerr << "command (" << command << "): " << x << std::endl;  return true; }
#define check_in_complete(x) { if (in.bad() || in.fail() || !in.eof()) error(x); }

namespace wf {
	namespace cuda {

		platform::platform(const std::vector<std::string> &args) : wf::platform("cuda") {
			for (auto arg : args)
				std::cerr << "Platform opengl does not support the argument " << arg << std::endl;
			register_batch_rt("simple",, simple_rt);
			register_batch_rt("if-if",, ifif);
			register_batch_rt("while-while",, whilewhile);
			register_batch_rt("persistent-if-if",, persistentifif);
			register_batch_rt("persistent-while-while",, persistentwhilewhile);
			register_batch_rt("speculative-while-while",, speculativewhilewhile);
			register_batch_rt("persistent-speculative-while-while",, persistentspeculativewhilewhile);
			register_batch_rt("dynamic-while-while",, dynamicwhilewhile);

			link_tracer("while-while", "default");
			link_tracer("while-while", "find closest hits");
			// bvh mode?
			register_rni_step_by_id(, initialize_framebuffer);
			register_rni_step_by_id(, batch_cam_ray_setup);
			//register_rni_step("store hitpoint albedo",, store_hitpoint_albedo_cpu);
			register_rni_step_by_id(, add_hitpoint_albedo_to_fb);
			register_rni_step_by_id(, download_framebuffer);
		}

		platform::~platform() {
			cudaDeviceReset();
		}

		void scenedata::upload(scene *scene) {
			std::vector<uint4> scene_tris;
			scene_tris.reserve(scene->triangles.size());
			for (triangle t : scene->triangles)
				scene_tris.push_back(uint4{t.a, t.b, t.c, t.material_id});
			triangles.upload(scene_tris.size(), reinterpret_cast<uint4*>(scene_tris.data()));

			int num_vertices = scene->vertices.size();
			std::vector<float4> tmp4(num_vertices);
			std::vector<float2> tmp2(num_vertices);

			for (int i = 0; i < num_vertices; ++i) {
				tmp4[i] = float4{ scene->vertices[i].pos.x, scene->vertices[i].pos.y, scene->vertices[i].pos.z, 0 };
				tmp2[i] = float2{ scene->vertices[i].tc.x, scene->vertices[i].tc.y };
			}
			vertex_pos.upload(tmp4);
			vertex_tc.upload(tmp2);

			auto f4 = [](const vec3 &v) { return float4{ v.x, v.y, v.z, 0 }; };
			std::vector<material> mtls(scene->materials.size());
			for (int i = 0; i < scene->materials.size(); ++i) {
				mtls[i].albedo = f4(scene->materials[i].albedo);
				mtls[i].emissive = f4(scene->materials[i].emissive);
				if (scene->materials[i].albedo_tex) {
					texture_image ti(*scene->materials[i].albedo_tex);
					tex_images.push_back(ti);
					mtls[i].albedo_tex = ti.tex;
				}
				else
					mtls[i].albedo_tex = 0;
			}
			materials.upload(mtls);
		}

		void batch_rt::build(::scene *scene)
		{
			rd = new raydata(rc->resolution());
			sd = new scenedata;

			binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on> bvh_rt;
			if (bvh_type == "sah")     bvh_rt.binary_split_type = bvh_rt.sah;
			else if (bvh_type == "sm") bvh_rt.binary_split_type = bvh_rt.sm;
			else if (bvh_type == "om") bvh_rt.binary_split_type = bvh_rt.om;
			bvh_rt.max_triangles_per_node = bvh_max_tris_per_node;
			bvh_rt.build(scene);

			// bvh_index.upload(bvh_rt.index);
			std::vector<uint1> new_index_list;
			for (auto index : bvh_rt.index) {
				uint1 new_index;
				new_index.x = index;
				new_index_list.push_back(new_index);
			}
			bvh_index.upload(new_index_list);

			bvh_nodes.upload(compact_bvh_node_builder::build(bvh_rt.nodes));

			auto *rt = dynamic_cast<batch_rt*>(rc->scene.batch_rt);
			assert(rt != nullptr);
			sd->upload(scene);
			std::cout << "upload done" << std::endl;
		}

		bool batch_rt::interprete(const std::string &command, std::istringstream &in) {
			if (command == "incoherence") {
				float in_r1, in_r2;
				in >> in_r1;
				in >> in_r2;
				check_in_complete("Syntax error, \"incoherence\" requires exactly two positive float values");
				if (in_r1 < 0 || in_r2 < 0)
					error("Parameter error, \"incoherence\" requires exactly two positive float values");
				if (in_r1 != 0 || in_r2 != 0) {
					use_incoherence = true;
					incoherence_r1 = in_r1;
					incoherence_r2 = in_r2;
				}
				return true;
			}
			else if (command == "bvh") {
				std::string sub;
				in >> sub;
				if (sub == "type") {
					std::string in1;
					in >> in1;
					check_in_complete("Syntax error, \"bvh type\" requires exactly one string value");
					bvh_type = in1;
					if (in1 != "sah" && in1 != "sm" && in1 != "om")
						error("Parameter error, \"bvh type\" must be one of \"sm\", \"om\", \"sah\"");
					return true;
				}
				else if (sub == "max_tris") {
					int in1;
					in >> in1;
					check_in_complete("Syntax error, \"bvh max_tris\" requires exactly one positive integer value");
					bvh_max_tris_per_node = in1;
					return true;
				}
			}
			return false;
		}

		__host__ std::vector<compact_bvh_node> compact_bvh_node_builder::build(std::vector<binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on>::node> nodes) {
			std::vector<wf::cuda::compact_bvh_node> nodes_new;
			for (const auto& n : nodes) {
				wf::cuda::compact_bvh_node node;
				node.data1 = make_float4(n.box_l.min.x, n.box_l.max.x, n.box_l.min.y, n.box_l.max.y);
				node.data2 = make_float4(n.box_r.min.x, n.box_r.max.x, n.box_r.min.y, n.box_r.max.y);
				node.data3 = make_float4(n.box_l.min.z, n.box_l.max.z, n.box_r.min.z, n.box_r.max.z);

				// change links on inner nodes to indicate wether child is inner node or leaf node
				if (n.inner()) {
					*(int*)&node.data4.x = nodes[n.link_l].inner() ? n.link_l : -n.link_l;
					*(int*)&node.data4.y = nodes[n.link_r].inner() ? n.link_r : -n.link_r;
				}
				else {
					*(int*)&node.data4.x = n.link_l;	// tri_offset
					*(int*)&node.data4.y = n.link_r;	// tri_count
				}
				nodes_new.push_back(node);
			}
			assert(nodes_new.size() == nodes.size());
			return nodes_new;
		}

	}
}
