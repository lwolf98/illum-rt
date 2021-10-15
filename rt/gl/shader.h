#pragma once

#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include <iostream>

class compute_shader {
	GLuint shader = 0,
		   program = 0;
public:
	std::string name, source;

	compute_shader(const std::string &name, const std::string &source = "") : name(name), source(source) {
	}
	void compile() {
		shader = glCreateShader(GL_COMPUTE_SHADER);
		const char *src = source.c_str();
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);
		
		GLint result;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
		if (result == GL_FALSE) {
			int length;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
			char* message = (char*)alloca(length * sizeof(char));
			glGetShaderInfoLog(shader, length, &length, message);
			glDeleteShader(shader);
			shader = 0;
			throw std::logic_error("Failed to compile shader '" + name + "'\n" + message);
		}

		program = glCreateProgram();
		glAttachShader(program, shader);
		glLinkProgram(program);
		
		glGetProgramiv(program, GL_LINK_STATUS, &result);
		if (result == GL_FALSE) {
			int length;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
			char* message = (char*)alloca(length * sizeof(char));
			glGetProgramInfoLog(program, length, &length, message);
			glDeleteShader(shader);
			glDeleteProgram(program);
			shader = program = 0;
			throw std::logic_error("Failed to link shader '" + name + "'\n" + message);
		}

		glValidateProgram(program);
		
		glGetProgramiv(program, GL_VALIDATE_STATUS, &result);
		if (result == GL_FALSE) {
			int length;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
			char* message = (char*)alloca(length * sizeof(char));
			glGetProgramInfoLog(program, length, &length, message);
			glDeleteShader(shader);
			glDeleteProgram(program);
			shader = program = 0;
			throw std::logic_error("Failed to validate shader '" + name + "'\n" + message);
		}
	}

	void bind() {
		glUseProgram(program);
	}
	void unbind() {
		glUseProgram(0);
	}

	void dispatch(int size_x, int size_y = 1, int size_z = 1) {
		glm::ivec3 local_size;
		glGetProgramiv(program, GL_COMPUTE_WORK_GROUP_SIZE, (GLint*)&local_size);
		float x = ((float)size_x)/local_size.x,
			  y = ((float)size_y)/local_size.y,
			  z = ((float)size_z)/local_size.z;
		int w = (int)x; if (floor(x) != x) ++w;
		int h = (int)y; if (floor(y) != y) ++h;
		int d = (int)z; if (floor(z) != z) ++d;
		glDispatchCompute(w, h, d);
	}

	int uniform_location(const std::string &name) {
		return glGetUniformLocation(program, name.c_str());
	}
	void uniform(const std::string &name, int x) {
		glUniform1i(uniform_location(name), x);
	}
};


