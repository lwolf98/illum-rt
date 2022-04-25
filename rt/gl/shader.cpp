#include "shader.h"

#include "libgi/timer.h"

#include <iostream>

using std::cout, std::endl, std::flush;

void compute_shader::compile() {
	time_this_block(shadercompiler);
	cout << "Compiling shader " << name << "..." << flush;
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

		int line = 2;
		cout << "1\t";
		for (int i = 0; i < source.length(); ++i)
			if (source[i] == '\n') cout << endl << line++ << '\t';
			else cout << source[i];
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
	cout << "done" << endl;
}

