#pragma once

#include <string>
#include <filesystem>
#include <glm/mat4x4.hpp>

enum class disp_action {
	map = 0,
	map_out = 1,
	uniform = 2,
	uniform_out = 3,
	random = 4,
	random_out = 5
};

struct load_config {
	std::filesystem::path model_path;
	std::string name;
	glm::mat4 model_matrix;
	uint32_t subd_level;
	bool subd_type_patches;
	std::string displacement_map;
	enum disp_action displace_action;
	float displacement_strength;
	int32_t bvh_align_level;

	load_config()
				: model_path(""),
				  name(""),
				  model_matrix(1),
				  subd_level(0),
				  subd_type_patches(true),
				  displacement_map(""),
				  displace_action(disp_action::map),
				  displacement_strength(0.f), // strength 0 means no displacement
				  bvh_align_level(-1) {}

	bool displace_out() {
		return static_cast<int>(displace_action) % 2;
	}

	bool is_displace_map() {
		return displace_action == disp_action::map || displace_action == disp_action::map_out;
	}

	void set_displace_action(std::string action) {
		if (action == "map")				displace_action = disp_action::map;
		else if (action == "map_out")		displace_action = disp_action::map_out;
		else if (action == "uniform")		displace_action = disp_action::uniform;
		else if (action == "uniform_out")	displace_action = disp_action::uniform_out;
		else if (action == "random")		displace_action = disp_action::random;
		else if (action == "random_out")	displace_action = disp_action::random_out;
	}

	bool operator==(const load_config &comp) const {
		bool result = true;
		result &= (model_path == comp.model_path);
		result &= (name == comp.name);
		result &= (subd_level == comp.subd_level);
		result &= (subd_type_patches == comp.subd_type_patches);
		result &= (displacement_map == comp.displacement_map);
		result &= (displacement_strength == comp.displacement_strength);
		result &= (bvh_align_level == comp.bvh_align_level);
		return result;
	}
};