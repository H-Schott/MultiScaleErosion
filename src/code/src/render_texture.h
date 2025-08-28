//
// Created by User on 2025/08/19.
//

#ifndef RENDER_TEXTURE_H
#define RENDER_TEXTURE_H

#include "GL/glew.h"
#include "stdio.h"
#include "shader-api.h"
#include "scalarfield2.h"

struct RenderTexture {
	GLuint fbo = 0;
	GLuint texture = 0;
	GLuint program = 0;
	int width, height;
};

inline RenderTexture create_render_texture(int width, int height, const char* shader_file) {
	RenderTexture rt;
	rt.width = width;
	rt.height = height;

	// Create texture
	glGenTextures(1, &rt.texture);
	glBindTexture(GL_TEXTURE_2D, rt.texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Create framebuffer
	glGenFramebuffers(1, &rt.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.texture, 0);

	// Check framebuffer completeness
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "Framebuffer not complete!\n");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	rt.program = read_program((std::string(shader_file).c_str()));

	return rt;
}

inline void set_render_texture_uniform2f(RenderTexture& rt, const char* uniform, float x, float y)
{
	glUseProgram(rt.program);
	glUniform2f(glGetUniformLocation(rt.program, uniform), x, y);
}

inline void set_render_texture_uniform3f(RenderTexture& rt, const char* uniform, float x, float y, float z)
{
	glUseProgram(rt.program);
	GLint location = glGetUniformLocation(rt.program, uniform);
	if (location == -1) {
		fprintf(stderr, "Uniform %s not found in shader program!\n", uniform);
		return;
	}
	glUniform3f(location, x, y, z);
}

inline void set_render_texture_uniform1i(RenderTexture& rt, const char* uniform, int i)
{
	glUseProgram(rt.program);
	glUniform1i(glGetUniformLocation(rt.program, uniform), i);
}

inline void render_to_texture(RenderTexture& rt, BufferDescriptor& buffer_desc) {
	GLuint buffer_ssbo = buffer_desc.id;
	// Save current viewport
	GLuint shader_program = rt.program;
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	if (rt.height != buffer_desc.ny || rt.width != buffer_desc.nx) {
		glBindTexture(GL_TEXTURE_2D, rt.texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, buffer_desc.nx, buffer_desc.ny, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	}

	// Bind framebuffer and set viewport
	glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
	glViewport(0, 0, buffer_desc.nx, buffer_desc.ny);

	// Clear and setup
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(shader_program);

	// Bind your buffer
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer_ssbo);

	glUniform2i(glGetUniformLocation(shader_program, "texSize"), buffer_desc.nx, buffer_desc.ny);
	// double zmin, zmax;
	double zrange[2];
	// buffer_info.GetRange(zrange[0], zrange[1]);
	glUniform2f(glGetUniformLocation(shader_program, "zRange"), float(buffer_desc.zmin), buffer_desc.zmax);

	glUniform1i(glGetUniformLocation(shader_program, "n_bands"), buffer_desc.n_bands);

	// Create and bind a dummy VAO (required for core profile)
	GLuint dummy_vao;
	glGenVertexArrays(1, &dummy_vao);
	glBindVertexArray(dummy_vao);

	// Draw fullscreen quad (4 vertices using gl_VertexID)
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// Cleanup
	glDeleteVertexArrays(1, &dummy_vao);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

inline void cleanup_render_texture(RenderTexture& rt) {
	glDeleteTextures(1, &rt.texture);
	glDeleteFramebuffers(1, &rt.fbo);
}

#endif //RENDER_TEXTURE_H
