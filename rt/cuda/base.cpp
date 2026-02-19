#include "config.h"
#include "base.h"
#include "platform.h"

#include "rt/cpu/bvh-ctor.h"

#include <string.h>
#include <sstream>
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
		
		template<typename T>
		std::string vecsize_string(const std::string &name, std::vector<T> vec, uint32_t *inc_size = nullptr) {
			uint32_t vec_size = vec.size();
			uint32_t type_size = sizeof(T);
			uint32_t total_size = vec_size * type_size;
			if (inc_size)
				*inc_size += total_size;
			//float size_mb = total_size * 1.f/1e6;

			double value = static_cast<double>(total_size);
			const char* unit = "B";
			uint32_t prec = 0;

			if (total_size >= 1'000'000) {
				value /= 1e6;
				unit = "MB";
				prec = 3;
			}
			else if (total_size >= 1'000) {
				value /= 1e3;
				unit = "kB";
				prec = 3;
			}

			std::stringstream sstr;
			sstr << std::left << std::setw(20) << (name + ":") << std::right
				 << "Count: " << std::setw(8) << vec_size
				 << " | Item size: " << std::setw(4) << type_size << " B"
				 << " | Total size: " << std::setw(8)
				 << std::fixed << std::setprecision(prec) << value << " " << unit;

			return sstr.str();
		}
		
		std::string size_string(const std::string &name, uint32_t total_size) {
			//uint32_t vec_size = vec.size();
			//uint32_t type_size = sizeof(T);
			//uint32_t total_size = vec_size * type_size;
			//if (out_size)
			//	*out_size = total_size;

			double value = static_cast<double>(total_size);
			const char* unit = "B";
			uint32_t prec = 0;

			if (total_size >= 1'000'000) {
				value /= 1e6;
				unit = "MB";
				prec = 3;
			}
			else if (total_size >= 1'000) {
				value /= 1e3;
				unit = "kB";
				prec = 3;
			}

			std::stringstream sstr;
			sstr << std::left << std::setw(20) << (name + ":") << std::right
				 << "Count: " << std::setw(8) << 1
				 << " | Item size: " << std::setw(4) << 0 << " B"
				 << " | Total size: " << std::setw(8)
				 << std::fixed << std::setprecision(prec) << value << " " << unit;

			return sstr.str();
		}

		void scenedata::upload(scene *scene) {
			std::stringstream profile;
			profile << "Device upload stats:" << std::endl;
			uint32_t total_size = 0;
			uint32_t part_size = 0;

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

			part_size = 0;
			profile << "Regular geometry:\n"
				<< "\t" << vecsize_string("Vertices", tmp_p, &part_size) << "\n"
				<< "\t" << vecsize_string("Triangles", scene_tris, &part_size) << "\n"
				<< "\t" << size_string("Total", part_size) << "\n"
				<< "\t" << "Copy:\n"
				<< "\t" << tmp_p.size() << "\n"
				<< "\t" << scene_tris.size() << "\n"
				<< std::endl;
			total_size += part_size;

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
			
			part_size = 0;
			profile << "Scene:\n"
				<< "\t" << vecsize_string("Materials", mtls, &part_size) << "\n"
				<< "\t" << "Copy:\n"
				<< "\t" << mtls.size() << "\n"
				<< std::endl;
			total_size += part_size;

			// SubD patches
			vector<subd_patch> device_patches(scene->patches.size());
			vector<subd_subpatch> device_subpatches;
			vector<patch_node> device_nodes;
			vector<aabb> device_root_boxes;
			tmp_p.clear(), tmp_n.clear(), tmp_t.clear();
			for (uint32_t i = 0; i < scene->patches.size(); ++i) {
				const subd::subd_patch &patch = scene->patches[i];
				auto &device_patch = device_patches[i];
				device_patch.subd_level = patch.subd_level;
				device_patch.material_id = patch.material_id;
#ifdef BOX_APPROXIMATION
				device_patch.subpatch_offset = device_subpatches.size();
				for (uint32_t n = 0; n < 4; ++n) {
					device_patch.box_tcs[n] = make_float2(patch.data[n].tc.x, patch.data[n].tc.y);
					device_patch.box_norms[n] = make_float4(patch.data[n].norm.x, patch.data[n].norm.y, patch.data[n].norm.z, 0.f);
				}
#endif

				// Resize subpatches and set offset
				uint32_t offset_subpatches = device_subpatches.size();
				device_subpatches.resize(offset_subpatches + patch.subpatches.size());
				device_root_boxes.resize(offset_subpatches + patch.subpatches.size());

				// TODO: ! Currently align_level -1 (meaning no box alignment) does not work on GPU, because this does not use subpatches !
				for (uint32_t j = 0; j < patch.subpatches.size(); ++j) {
					const subd::subd_subpatch &subpatch = patch.subpatches[j];
					auto &device_subpatch = device_subpatches[offset_subpatches+j];
					auto &device_subpatch_root_box = device_root_boxes[offset_subpatches+j];
					device_subpatch_root_box = subpatch.root_box_world; //aabb root_box from parent patch (not subpatch.root_box)

					device_subpatch.parent_id = i; // reference to parent patch
					device_subpatch.vert_start = subpatch.vert_start;
					device_subpatch.trafo = mat3::from(subpatch.trafo);
#ifdef PROJECTION
					device_subpatch.proj = mat3::from(subpatch.proj);
	#ifdef PRECALC_INV_PROJ_MATRIX
					device_subpatch.proj_inv = mat3::from(subpatch.proj_inv);
	#endif
#endif
					device_subpatch.subd_level = subpatch.subd_level;
#ifdef BOX_APPROXIMATION
	#ifndef PROJECTION
					device_subpatch.root_min = f4(subpatch.root_box.min); //-> REVIEW: required for box approximation (probably better to use two float4)
					device_subpatch.root_max = f4(subpatch.root_box.max);
	#else
					device_subpatch.root_min_y = subpatch.root_box.min.y;
					device_subpatch.root_max_y = subpatch.root_box.max.y;
	#endif
#endif

					// Resize nodes and set offset
					uint32_t offset_nodes = device_nodes.size();
					device_nodes.resize(offset_nodes + subpatch.nodes.size());
					device_subpatch.bvh_node_offset = offset_nodes;

					#pragma omp parallel for
					for (uint32_t k = 0; k < subpatch.nodes.size(); ++k) {
#if !defined(SLAB_COMPRESSION) && !defined(QUANTIZATION)
						const subd::patch_base_node &node = subpatch.nodes[k]; //REVIEW: BASE?
						patch_node &device_node = device_nodes[offset_nodes+k];
						device_node.set_min(0, node.boxes[0].min);
						device_node.set_min(1, node.boxes[1].min);
						device_node.set_min(2, node.boxes[2].min);
						device_node.set_min(3, node.boxes[3].min);
						device_node.set_max(0, node.boxes[0].max);
						device_node.set_max(1, node.boxes[1].max);
						device_node.set_max(2, node.boxes[2].max);
						device_node.set_max(3, node.boxes[3].max);
#else
						const subd::patch_slab_node &node = subpatch.nodes[k];
						patch_node &device_node = device_nodes[offset_nodes+k];
						//device_node = patch_node::from(node);
						//device_node = node;
						device_node = patch_node::copy(node); //REVIEW: also possible to just assign?
#endif
					}
				}

				device_patch.start_index = tmp_p.size();
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

			// Store patch data
			if (device_patches.size() > 0) {
				patches.upload(device_patches);
				subpatches.upload(device_subpatches);
				patch_nodes.upload(device_nodes);
				patch_root_boxes.upload(device_root_boxes);

#if !defined(BOX_APPROXIMATION) || defined(KEEP_GEOMETRY)
				patch_vertex_pos.upload(tmp_p);
				patch_vertex_norm.upload(tmp_n);
				patch_vertex_tc.upload(tmp_t);
#endif
			}

			part_size = 0;
			profile << "Patch geometry:\n"
				<< "\t" << vecsize_string("Patches", device_patches, &part_size) << "\n"
				<< "\t" << vecsize_string("Subpatches", device_subpatches, &part_size) << "\n"
				<< "\t" << vecsize_string("Subpatch nodes", device_nodes, &part_size) << "\n"
				<< "\t" << vecsize_string("Patch root boxes", device_root_boxes, &part_size) << "\n"
#if !defined(BOX_APPROXIMATION) || defined(KEEP_GEOMETRY)
				<< "\t" << vecsize_string("Patch vertices", tmp_p, &part_size) << "\n"
#endif
				<< "\t" << size_string("Total", part_size) << "\n"
				<< "\t" << "Copy:\n"
				<< "\t" << device_patches.size() << "\n"
				<< "\t" << device_subpatches.size() << "\n"
				<< "\t" << device_nodes.size() << "\n"
				<< "\t" << device_root_boxes.size() << "\n"
#if !defined(BOX_APPROXIMATION) || defined(KEEP_GEOMETRY)
				<< "\t" << tmp_p.size() << "\n"
#endif
				<< std::endl;

			total_size += part_size;
			profile << "Summary:\n"
				<< "\t" << size_string("Total", total_size) << "\n";

			std::cout << profile.str();
			std::string outfile = "out_profiling/" + rc->outfile_full(".txt", true, true);
			//std::cout << "Test path: " << rc->outfile_full(".txt", true) << std::endl;
			std::cout << "Test path: " << outfile << std::endl;
			std::ofstream out(outfile);
			if (out) {
				out << outfile;
				out << "\n" << "----------------------------------------------------------------" << "\n\n";
				out << profile.str();
				out.close();
			}
			else {
				std::cout << "Error opening file: " << outfile << std::endl;
			}

			// load scene_refs object

			scene_refs device_refs;
			device_refs.vertex_pos = vertex_pos.device_memory;
			device_refs.vertex_norm = vertex_norm.device_memory;
			device_refs.vertex_tc = vertex_tc.device_memory;
			device_refs.triangles = triangles.device_memory;
			device_refs.materials = materials.device_memory;
			//device_refs.tex_images = tex_images.device_memory;

			device_refs.patches = patches.device_memory;
#if !defined(BOX_APPROXIMATION) || defined(KEEP_GEOMETRY)
			device_refs.patch_vertex_pos = patch_vertex_pos.device_memory;
			device_refs.patch_vertex_norm = patch_vertex_norm.device_memory;
			device_refs.patch_vertex_tc = patch_vertex_tc.device_memory;
#endif
			device_refs.subpatches = subpatches.device_memory;
			device_refs.patch_nodes = patch_nodes.device_memory;
			refs.upload(1, &device_refs);
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
