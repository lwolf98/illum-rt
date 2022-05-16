#include "base.h"
#include "platform.h"

#include "rt/cpu/bvh-ctor.h"

#include <iostream>

#define error(x) { cerr << "command (" << command << "): " << x << endl;  return true; }
#define check_in_complete(x) { if (in.bad() || in.fail() || !in.eof()) error(x); }

using namespace std;

namespace wf {
	namespace cuda {

		void timer::start(const std::string &name) {
			cudaEvent_t start, stop;
			if (events.find(name) == events.end()) {
				cudaEventCreate(&start);
				cudaEventCreate(&stop);
				events[name] = { start, stop };
			}
			else
				start = events[name].first;
			cudaEventRecord(start);
		}

		void timer::stop(const std::string &name) {
			cudaEvent_t stop = events[name].second;
			cudaEventRecord(stop);
		}

		void timer::synchronize() {
			for (auto [name,ev] : events) {
				auto [start,stop] = ev;
				float milliseconds = 0;
				cudaEventElapsedTime(&milliseconds, start, stop);

				// funnel to stats_timer
				stats_timer.timers[0].times[name] += milliseconds * 1000 * 1000;
				stats_timer.timers[0].counts[name]++;
			}
			events.clear();
		}
		
	
		void scenedata::upload(scene *scene) {
			vector<uint4> scene_tris;
			scene_tris.reserve(scene->triangles.size());
			for (triangle t : scene->triangles)
				scene_tris.push_back(uint4{t.a, t.b, t.c, t.material_id});
			triangles.upload(scene_tris.size(), reinterpret_cast<uint4*>(scene_tris.data()));

			n_vertices = scene->vertices.size();
			n_triangles = scene->triangles.size();
			vector<float4> tmp4(n_vertices);
			vector<float2> tmp2(n_vertices);

			for (int i = 0; i < n_vertices; ++i) {
				tmp4[i] = float4{ scene->vertices[i].pos.x, scene->vertices[i].pos.y, scene->vertices[i].pos.z, 0 };
				tmp2[i] = float2{ scene->vertices[i].tc.x, scene->vertices[i].tc.y };
			}
			vertex_pos.upload(tmp4);
			vertex_tc.upload(tmp2);

			auto f4 = [](const vec3 &v) { return float4{ v.x, v.y, v.z, 0 }; };
			vector<material> mtls(scene->materials.size());
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

		void batch_rt::build(scenedata *scene)
		{
			rd = new raydata(rc->resolution());

			scene->triangles.download();
			scene->vertex_pos.download();
			cpu_bvh_builder_cuda_scene_traits st { scene };

			bvh_ctor<bbvh_triangle_layout::indexed, cpu_bvh_builder_cuda_scene_traits> *ctor = nullptr;
			if (bvh_type == "sah")     ctor = new bvh_ctor_sah<bbvh_triangle_layout::indexed, cpu_bvh_builder_cuda_scene_traits>(st, bvh_max_tris_per_node, 16);
			else if (bvh_type == "sm") ctor = new bvh_ctor_sm <bbvh_triangle_layout::indexed, cpu_bvh_builder_cuda_scene_traits>(st, bvh_max_tris_per_node);
			else if (bvh_type == "om") ctor = new bvh_ctor_om <bbvh_triangle_layout::indexed, cpu_bvh_builder_cuda_scene_traits>(st, bvh_max_tris_per_node);
			::bvh bvh = ctor->build(true);

			// HACK: due to "scene views" the current scenedata* might not own the vertex data
			scenedata *org_scene = scene;
			while (org_scene->org) org_scene = org_scene->org;
			org_scene->triangles.upload(scene->triangles.host_data);

			bvh_index.upload(bvh.index);
			bvh_nodes.upload(compact_bvh_node_builder::build(bvh.nodes));

			scene->triangles.free_host_data();
			bvh_index.free_host_data();
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
				string sub;
				in >> sub;
				if (sub == "type") {
					string in1;
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

		std::vector<compact_bvh_node> compact_bvh_node_builder::build(std::vector<binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on>::node> nodes) {
			vector<wf::cuda::compact_bvh_node> nodes_new;
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
