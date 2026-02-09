#pragma once

#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <vector>
#include "libgi/rt.h"

/**
 * based on: https://schneide.blog/2016/07/15/generating-an-icosphere-in-c/
 */
namespace objdraw {
	const float X=.525731112119133606f;
	const float Z=.850650808352039932f;
	const float N=0.f;

	static const std::vector<vec3> vertices = {
		{-X,N,Z}, {X,N,Z}, {-X,N,-Z}, {X,N,-Z},
		{N,Z,X}, {N,Z,-X}, {N,-Z,X}, {N,-Z,-X},
		{Z,X,N}, {-Z,X, N}, {Z,-X,N}, {-Z,-X, N}
	};

	static const std::vector<triangle> triangles =
	{
		{1,5,2},{1,10,5},{10,6,5},{5,6,9},{5,9,2},
		{9,11,2},{9,4,11},{6,4,9},{6,3,4},{3,8,4},
		{8,11,4},{8,7,11},{8,12,7},{12,1,7},{1,2,7},
		{7,2,11},{10,1,12},{10,12,3},{10,3,6},{8,3,12}
	};

	class icosphere {
	private:
		vec3 pos;
		float scale;

	public:
		icosphere(const vec3& pos, float scale) : pos(pos), scale(scale) {}
		icosphere(const vec3& pos) : icosphere(pos, 1.0f) {}
		icosphere() : icosphere(vec3(0), 1.0f) {}
		std::string obj_string(int32_t& start);
	};

	class path {
	private:
		std::vector<vec3> vertices;

	public:
		path();
		path(const vec3& start_vertex);
		void push_vertex(const vec3& v);
		std::string obj_string(int32_t& start);
	};

	class obj_writer {
	private:
		std::ofstream out;
		int32_t start;
		int32_t off;

	public:
		obj_writer(const std::string& out_file)
		: out(std::ofstream(out_file)), start(0), off(0) {}

		void write_path(path path);
		void write_icosphere(icosphere ico);
	};
}