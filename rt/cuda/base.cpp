#include "config.h"
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
			vector<float4> tmp_p(n_vertices);
			vector<float4> tmp_n(n_vertices);
			vector<float2> tmp_t(n_vertices);

			for (int i = 0; i < n_vertices; ++i) {
				tmp_p[i] = float4{ scene->vertices[i].pos.x,  scene->vertices[i].pos.y,  scene->vertices[i].pos.z,  1 };
				tmp_n[i] = float4{ scene->vertices[i].norm.x, scene->vertices[i].norm.y, scene->vertices[i].norm.z, 0 };
				tmp_t[i] = float2{ scene->vertices[i].tc.x, scene->vertices[i].tc.y };
			}
			vertex_pos.upload(tmp_p);
			vertex_norm.upload(tmp_n);
			vertex_tc.upload(tmp_t);

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
				mtls[i].ior = scene->materials[i].ior;
				mtls[i].roughness = scene->materials[i].roughness;
			}
			materials.upload(mtls);

			// SubD patches
			vector<subd_patch> device_patches(scene->patches.size());
			vector<patch_node> device_nodes;
			vector<aabb> device_root_nodes(scene->patches.size());
			tmp_p.clear(), tmp_n.clear(), tmp_t.clear();
			for (int i = 0; i < scene->patches.size(); ++i) {
				const auto &patch = scene->patches[i];
				auto &device_patch = device_patches[i];
				device_patch.subd_level = patch.subd_level;
				device_patch.material_id = patch.material_id;
				
				uint32_t offset = device_nodes.size();
				device_nodes.resize(offset + patch.nodes.size());
				device_patch.bvh_node = offset + patch.bvh_node;

				//TODO: remove min and max from patch when patch_root_nodes structure is used
				device_patch.min = f4(patch.nodes[patch.bvh_node].box.min);
				device_patch.max = f4(patch.nodes[patch.bvh_node].box.max);
				device_root_nodes[i] = patch.nodes[patch.bvh_node].box;

				#pragma omp parallel for
				for (int j = 0; j < patch.nodes.size(); ++j) {
					patch_node device_node;
					const subd::node &node = patch.nodes[j];
					device_node.min = f4(node.box.min);
					device_node.max = f4(node.box.max);
					device_node.left = offset + node.left;
					device_node.right = offset + node.right;
					device_node.triangle = node.triangle;

					//device_nodes.push_back(device_node);
					device_nodes[offset + j] = device_node;
				}

				device_patch.start_index = patch.verts.size();
				uint32_t n_device_verts = tmp_p.size() + patch.verts.size();
				tmp_p.resize(n_device_verts);
				tmp_n.resize(n_device_verts);
				tmp_t.resize(n_device_verts);

				#pragma omp parallel for
				for (uint32_t j = 0; j < patch.verts.size(); ++j) {
					uint32_t insert_index = device_patch.start_index + j;
					tmp_p[insert_index] = float4{ patch.verts[j].pos.x,  patch.verts[j].pos.y,  patch.verts[j].pos.z,  1 };
					tmp_n[insert_index] = float4{ patch.verts[j].norm.x, patch.verts[j].norm.y, patch.verts[j].norm.z, 0 };
					tmp_t[insert_index] = float2{ patch.verts[j].tc.x, patch.verts[j].tc.y };
				}

			}

			patches.upload(device_patches);
			patch_nodes.upload(device_nodes);
			patch_root_nodes.upload(device_root_nodes);

			patch_vertex_pos.upload(tmp_p);
			patch_vertex_norm.upload(tmp_n);
			patch_vertex_tc.upload(tmp_t);
		}

		void batch_rt::build(scenedata *scene)
		{
			scene->triangles.download();
			scene->vertex_pos.download();
			cpu_bvh_builder_cuda_scene_traits st { scene };

			bvh_ctor<bbvh_triangle_layout::indexed, cpu_bvh_builder_cuda_scene_traits> *ctor = nullptr;
			bool enable_esc = true;
			if (bvh_type == "sah")     ctor = new bvh_ctor_sah<bbvh_triangle_layout::indexed, cpu_bvh_builder_cuda_scene_traits>(st, bvh_max_tris_per_node, 16);
			else if (bvh_type == "sm") ctor = new bvh_ctor_sm <bbvh_triangle_layout::indexed, cpu_bvh_builder_cuda_scene_traits>(st, bvh_max_tris_per_node);
			else if (bvh_type == "om") ctor = new bvh_ctor_om <bbvh_triangle_layout::indexed, cpu_bvh_builder_cuda_scene_traits>(st, bvh_max_tris_per_node);
			#ifdef HAVE_LIBEMBREE3
			else if (bvh_type == "embree") {
				ctor = new bvh_ctor_embree <bbvh_triangle_layout::indexed, cpu_bvh_builder_cuda_scene_traits>(st, bvh_max_tris_per_node);
				enable_esc = false;
			}
			#endif
			::bvh bvh = ctor->build(enable_esc);

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
					if (in1 != "sah" && in1 != "sm" && in1 != "om" && in1 != "embree")
						error("Parameter error, \"bvh type\" must be one of \"sm\", \"om\", \"sah\", \"embree\"");
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
