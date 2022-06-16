#include "opengl_api.h"
#include "glad/glad.h"
#include "glm/gtc/type_ptr.hpp"
#include <SDL2/SDL_image.h>
#include <fstream>
#include <cassert>
#include <iostream>

void Framebuffer::create(FramebufferSpec n_spec)
{
	spec = n_spec;

	invalidate();
}
void Framebuffer::invalidate()
{
	if (ID != 0) {
		glDeleteFramebuffers(1, &ID);
		glDeleteTextures(1, &color_id);
		glDeleteTextures(1, &depth_stencil_id);
		glDeleteTextures(1, &depth_id);

		depth_id = 0;
		color_id = 0;
		depth_stencil_id = 0;
	}
	glGenFramebuffers(1, &ID);
	glBindFramebuffer(GL_FRAMEBUFFER, ID);

	bool multisample = spec.samples > 1;

	static const GLenum to_glenum_if[] = { 0,0,GL_RGBA, GL_RGB,GL_RGBA16F,GL_RGBA32F };
	// color texture
	if (spec.attach != FBAttachments::a_none && spec.attach != FBAttachments::a_depth) {
		const GLenum internal_format = to_glenum_if[static_cast<int>(spec.attach)];
		const GLenum target = (multisample) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
		const GLenum type = (internal_format == GL_RGBA16F || internal_format == GL_RGBA32F) ? GL_FLOAT : GL_UNSIGNED_BYTE;
		const GLenum format = (internal_format == GL_RGB) ? GL_RGB : GL_RGBA;
		glGenTextures(1, &color_id);
		glBindTexture(target, color_id);
		if (multisample) {
			glTexImage2DMultisample(target, spec.samples, internal_format, spec.width, spec.height, GL_TRUE);
		}
		else {
			glTexImage2D(target, 0, internal_format, spec.width, spec.height, 0, format, type, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, color_id, 0);
	}
	// depth texture
	if (spec.attach == FBAttachments::a_depth) {
		assert(spec.samples == 1);

		glGenTextures(1, &depth_id);
		glBindTexture(GL_TEXTURE_2D, depth_id);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, spec.width, spec.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_id, 0);
	}
	// depth/stencil texture
	if (spec.has_depth_stencil) {

		//glGenRenderbuffers(1, &d_s_rbo);
		//glBindRenderbuffer(GL_RENDERBUFFER, d_s_rbo);
		
		glGenTextures(1, &depth_stencil_id);
		const GLenum target = (multisample) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
		glBindTexture(target, depth_stencil_id);

		if (multisample) {
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, spec.samples, GL_DEPTH24_STENCIL8,spec.width,spec.height,GL_TRUE);

			//glRenderbufferStorageMultisample(GL_RENDERBUFFER, spec.samples, GL_DEPTH24_STENCIL8, spec.width, spec.height);

		}
		else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, spec.width, spec.height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			//glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, spec.width, spec.height);
		}

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, target, depth_stencil_id, 0);
		//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, d_s_rbo);
	}


	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void Framebuffer::update_window_size(int n_width, int n_height)
{
	spec.width = n_width;
	spec.height = n_height;
	
	invalidate();
}

void Framebuffer::bind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, ID);
}
void Framebuffer::destroy()
{
	glDeleteFramebuffers(1, &ID);
	glDeleteTextures(1, &color_id);
	glDeleteTextures(1, &depth_stencil_id);
	glDeleteTextures(1, &depth_id);

	ID = 0;
	depth_id = 0;
	color_id = 0;
	depth_stencil_id = 0;
}

const char* shader_path = "resources/shaders/";
Shader::Shader(const char* vertex_path, const char* fragment_path, const char* geo_path)
{
	ID = load_shader(vertex_path, fragment_path, geo_path);
}
void Shader::use()
{
	glUseProgram(ID);
}
Shader& Shader::set_bool(const char* name, bool value)
{
	glUniform1i(glGetUniformLocation(ID, name), (int)value);
	return *this;
}
Shader& Shader::set_int(const char* name, int value)
{
	glUniform1i(glGetUniformLocation(ID, name), value);
	return *this;
}
Shader& Shader::set_float(const char* name, float value)
{
	glUniform1f(glGetUniformLocation(ID, name), value);
	return *this;
}
Shader& Shader::set_mat4(const char* name, mat4 value)
{
	glUniformMatrix4fv(glGetUniformLocation(ID, name), 1, GL_FALSE, glm::value_ptr(value));
	return *this;
}
Shader& Shader::set_vec3(const char* name, vec3 value)
{
	glUniform3f(glGetUniformLocation(ID, name), value.r, value.g, value.b);
	return *this;
}
std::string Shader::read_file(const char* filepath)
{
	std::string true_path(shader_path);
	true_path += filepath;
	std::ifstream infile(true_path);
	if (!infile) {
		printf("Coundn't open shader file: %s\n", filepath);
		return "error";
	}
	return std::string((std::istreambuf_iterator<char>(infile)),
		std::istreambuf_iterator<char>());
}
unsigned int Shader::load_shader(const char* vertex_path, const char* fragment_path, const char* geo_path)
{
	unsigned int vertex;
	unsigned int fragment;
	unsigned int geometry = 0;
	unsigned int program;
	std::string v_string = read_file(vertex_path);
	std::string f_string = read_file(fragment_path);
	const char* vertex_source = v_string.c_str();
	const char* fragment_source = f_string.c_str();
	int success = 0;
	char infolog[512];

	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vertex_source, NULL);
	glCompileShader(vertex);

	glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertex, 512, NULL, infolog);
		printf("Error: vertex shader compiliation failed: %s", infolog);
	}

	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fragment_source, NULL);
	glCompileShader(fragment);

	glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragment, 512, NULL, infolog);
		printf("Error: fragment shader compiliation failed: %s", infolog);
	}
	if (geo_path != NULL) {
		std::string g_string = read_file(geo_path);
		const char* geo_source = g_string.c_str();
		geometry = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(geometry, 1, &geo_source, NULL);
		glCompileShader(geometry);

		glGetShaderiv(geometry, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(geometry, 512, NULL, infolog);
			printf("Error: geometry shader compiliation failed: %s", infolog);
		}
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	if (geo_path != NULL) {
		glAttachShader(program, geometry);
	}
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infolog);
		printf("Error: shader program link failed: %s", infolog);
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);
	glDeleteShader(geometry);

	return program;
}
void clear_screen(bool color, bool depth)
{
	glClear(
		((color) ? GL_COLOR_BUFFER_BIT : 0) | 
		((depth) ? GL_DEPTH_BUFFER_BIT : 0));
}
void set_sampler2d(uint32_t binding, uint32_t id) {
	glActiveTexture(GL_TEXTURE0 + binding);
	glBindTexture(GL_TEXTURE_2D, id);
}
VertexArray::~VertexArray()
{
	glDeleteBuffers(1, &VBO);
	glDeleteVertexArrays(1, &VAO);
}
VertexArray::VertexArray()
{
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	// POSITION
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexP), (void*)0);
	glEnableVertexAttribArray(0);
	// UV
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexP), (void*)(3*sizeof(float)));
	glEnableVertexAttribArray(1);
	// COLOR
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VertexP), (void*)(5 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}

void VertexArray::draw_array()
{
	static const GLenum to_glenum[] = { GL_TRIANGLES, GL_LINES, GL_LINE_STRIP, GL_POINTS };
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	if (verts.size() > allocated_size) {
		allocated_size = verts.size() * 1.5;
		glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(VertexP), verts.data(), GL_STATIC_DRAW);
	}
	else {
		glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size()*sizeof(VertexP), verts.data());
	}
	glDrawArrays(to_glenum[type], 0, verts.size());
}
void VertexArray::upload_data()
{
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	if (verts.size() > allocated_size) {
		allocated_size = verts.size() * 1.5;
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(VertexP), verts.data(), GL_STATIC_DRAW);
	}
	else {
		glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(VertexP), verts.data());
	}
}
void VertexArray::draw_array_static()
{
	static const GLenum to_glenum[] = { GL_TRIANGLES, GL_LINES, GL_LINE_STRIP, GL_POINTS };
	glDrawArrays(to_glenum[type], 0, verts.size());
}
void VertexArray::add_quad(vec2 upper, vec2 size)
{
	VertexP corners[4];
	corners[0].position = vec3(upper,0);
	corners[1].position = vec3(upper.x + size.x, upper.y, 0);
	corners[2].position = vec3(upper.x + size.x, upper.y-size.y, 0);
	corners[3].position = vec3(upper.x, upper.y -size.y, 0);
	corners[3].uv = vec2(0);
	corners[2].uv = vec2(1,0);
	corners[1].uv = vec2(1, 1);
	corners[0].uv = vec2(0, 1);


	push_3(corners[0], corners[1], corners[2]);
	push_3(corners[0], corners[2], corners[3]);
}