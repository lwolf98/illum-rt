#include "shader.h"

#include <iostream>

namespace gl {

	void shader::compile_shader(const std::string &shader_src, GLenum shader_type) {

		GLuint shader = glCreateShader(shader_type);
		const char *src = shader_src.c_str();
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

			int line = 2;
			std::cout << "1\t";
			for (int i = 0; i < shader_src.length(); ++i)
				if (shader_src[i] == '\n') std::cout << std::endl << line++ << '\t';
				else std::cout << shader_src[i];
			throw std::logic_error("Failed to compile shader '" + name + "'\n" + message);
		}

		if (!program) program = glCreateProgram();
		glAttachShader(program, shader);
		shaders.push_back(shader);
	}

	void shader::link_and_validate() {

		glLinkProgram(program);

		GLint result;
		glGetProgramiv(program, GL_LINK_STATUS, &result);
		if (result == GL_FALSE) {
			int length;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
			char* message = (char*)alloca(length * sizeof(char));
			glGetProgramInfoLog(program, length, &length, message);
			delete_and_detatch_shaders();
			glDeleteProgram(program);
			program = 0;
			throw std::logic_error("Failed to link shader '" + name + "'\n" + message);
		}

		glValidateProgram(program);

		glGetProgramiv(program, GL_VALIDATE_STATUS, &result);
		if (result == GL_FALSE) {
			int length;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
			char* message = (char*)alloca(length * sizeof(char));
			glGetProgramInfoLog(program, length, &length, message);
			delete_and_detatch_shaders();
			glDeleteProgram(program);
			program = 0;
			throw std::logic_error("Failed to validate shader '" + name + "'\n" + message);
		}

		delete_and_detatch_shaders();
	}

	void compute_shader::dispatch(int size_x, int size_y, int size_z) {
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
}
