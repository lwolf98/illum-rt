#pragma once

#include "algorithm.h"

#include <glm/glm.hpp>
#include <png++/png.hpp>
#include <functional>

//! A 2D buffer with parallel operations
template<typename T> struct buffer {
	unsigned w, h;
	T *data = nullptr;
	buffer(unsigned w, unsigned h) : w(w), h(h) {
		data = new T[w*h];
	}
	buffer(const buffer &) = delete;
	buffer(buffer &&) = delete;
	~buffer() {
		delete [] data;
	}
	T& operator()(unsigned x, unsigned y) { return data[y*w+x]; }
	const T& operator()(unsigned x, unsigned y) const { return data[y*w+x]; }
	buffer& operator=(buffer &other) = delete;
	buffer& operator=(buffer &&other) {
		std::swap(w, other.w);
		std::swap(h, other.h);
		std::swap(data, other.data);
		return *this;
	}
	void clear(const T &to) {
		#pragma omp parallel for
		for (int i = 0; i < w*h; ++i)
			data[i] = to;
	}
	//! RUn function \c fn for each entry in the 2D grid
	void for_each(const std::function<void(unsigned x, unsigned y)> &fn) const {
		#pragma omp parallel for
		for (unsigned y = 0; y < h; ++y)
			for (unsigned x = 0; x < w; ++x)
				fn(x, y);
	}
};

class framebuffer {
public:
	buffer<vec4> color;
	framebuffer(unsigned w, unsigned h) : color(w, h) {
	}
	~framebuffer() {
	}
	void resize(unsigned new_w, unsigned new_h) {
		color = buffer<vec4>(new_w, new_h);
	}
	void clear();
	void add(unsigned x, unsigned y, gi_algorithm::sample_result res);
	png::image<png::rgb_pixel> png() const;
};
