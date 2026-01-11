#pragma once

#include <string>
#include <filesystem>
#include <glm/mat4x4.hpp>

struct load_config {
	std::filesystem::path model_path;
	std::string name;
	glm::mat4 model_matrix;
	uint32_t subd_level;
	bool subd_type_patches;
	std::string displacement_map;
	std::string displace_action;
	float displacement_strength;
	int32_t bvh_align_level;

	load_config()
				: model_path(""),
				  name(""),
				  model_matrix(1),
				  subd_level(0),
				  subd_type_patches(true),
				  displacement_map(""),
				  displace_action("map"),
				  displacement_strength(0.f), // strength 0 means no displacement
				  bvh_align_level(-1) {}
};