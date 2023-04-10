#include "preprocessing.h"

#include "libgi/color.h"

#include <glm/gtx/transform.hpp>

namespace wf::cuda {
		
	
	void build_accel_struct::run() {
		pf->rt->build(pf->sd);
	}

	compute_light_distribution::compute_light_distribution()
	: f("light dist / f", 1),
	  cdf("light dist / cdf", 1),
	  tri_lights("light dist / tri lights", 1) {
	}

	void compute_light_distribution::run() {
		std::vector<float> light_power;
		std::vector<uint4> tri_light_host;
		for (int i = 0; i < ::rc->scene.lights.size(); ++i)
			if (auto *light = dynamic_cast<trianglelight*>(::rc->scene.lights[i])) {
				tri_light_host.push_back(uint4{light->a, light->b, light->c, light->material_id});
				light_power.push_back(luma(light->power()));
			}
		tri_lights.upload(tri_light_host);

		distribution_1d dist(light_power);
		
		f.upload(dist.f);
		cdf.upload(dist.cdf);
		integral_1spaced = dist.integral_1spaced;
		n = tri_light_host.size();
	}

	namespace k {
		void rotate_scene(const glm::mat4 &rot, float4 *vertex_pos_dst, const float4 *vertex_pos_src, float4 *vertex_norm, int vertices);
	}
	void rotate_scene::run() {
		float rot_x = pi/4;
		float rot_y = pi/4;
		float rot_z = pi/4;
		glm::mat4 m_x = glm::rotate(rot_x, vec3(1, 0, 0));
		glm::mat4 m_y = glm::rotate(rot_y, vec3(0, 1, 0));
		glm::mat4 m_z = glm::rotate(rot_z, vec3(0, 0, 1));
		glm::mat4 rot = m_z * m_y * m_x;

		k::rotate_scene(rot, pf->sd->vertex_pos.device_memory, pf->sd->vertex_pos.device_memory, nullptr, pf->sd->n_vertices);
	}
	
	void rotate_scene_keep_org::run() {
		float rot_x = pi/4;
		float rot_y = pi/4;
		float rot_z = pi/4;
		glm::mat4 m_x = glm::rotate(rot_x, vec3(1, 0, 0));
		glm::mat4 m_y = glm::rotate(rot_y, vec3(0, 1, 0));
		glm::mat4 m_z = glm::rotate(rot_z, vec3(0, 0, 1));
		glm::mat4 rot = m_z * m_y * m_x;

		pf->sd = new scenedata(pf->sd, shallow_non_owning_copy);
		pf->sd->vertex_pos = texture_buffer<float4>(pf->sd->vertex_pos, mem_duplicating_copy_only);
		k::rotate_scene(rot, pf->sd->vertex_pos.device_memory, pf->sd->org->vertex_pos.device_memory, nullptr, pf->sd->n_vertices);
	}

	void drop_scene_view::run() {
		assert(pf->sd->org);
		scenedata *view = pf->sd;
		pf->sd = view->org;
		delete view;
	}
}

