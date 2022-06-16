#ifndef OPENGL_API_H
#define OPENGL_API_H

#include "glm/glm.hpp"
#include "geometry.h"
#include <string>
#include <vector>
using namespace glm;

struct Vertex
{
	Vertex() {}
	Vertex(vec3 position, vec3 normal, vec2 uv) : position(position), normal(normal), uv(uv) {}
	vec3 position = vec3(0); 
	vec3 normal = vec3(0);
	vec2 uv = vec2(0);
};
enum class Uniforms
{
	u_viewpos,
	u_viewdir,
	
	u_view_matrix,
	u_projection_matrix,
	u_model_matrix,
	u_inverse_model_matrix,
	u_specular_exp,

	u_time,


};

struct Shader
{
	uint32_t ID=0;

	Shader(const char* vertex_path, const char* fragment_path, const char* geo_path = NULL);
	Shader(){}

	void use();
	Shader& set_bool(const char* name, bool value);
	Shader& set_int(const char* name, int value);
	Shader& set_float(const char* name, float value);
	Shader& set_mat4(const char* name, mat4 value);
	Shader& set_vec3(const char* name, vec3 value);
private:
	std::string read_file(const char* filepath);
	unsigned int load_shader(const char* vertex_path, const char* fragment_path, const char* geo_path = NULL);
};

// Vertex primitive, for drawing things like lines, triangles, quads, etc.
struct VertexP
{
	VertexP(vec3 pos, vec3 color) {
		position = pos;
		r = (color.r > 1) ? 255 : color.r * 255;
		g = (color.g > 1) ? 255 : color.g * 255;
		b = (color.b > 1) ? 255 : color.b * 255;
	}
	VertexP() {}
	vec3 position = vec3(0);
	vec2 uv = vec2(0);
	uint8_t r=255, g=255, b=255, a=255;
};

// A dynamic array suited for additons/deletions on a smaller scale
// Good for 2d elements like sprites or UI
struct VertexArray
{
	VertexArray();
	~VertexArray();

	enum Primitive
	{
		triangle,
		lines,
		line_strip,
		points,
	};

	void draw_array();
	void append(VertexP p) {
		verts.push_back(p);
	}
	void resize(uint32_t new_size) {
		verts.resize(new_size);
	}
	void set_primitive(Primitive new_type) {
		type = new_type;
	}
	int get_size() {
		return verts.size();
	}
	VertexP& operator[](uint32_t index) {
		return verts.at(index);
	}
	void push_3(VertexP a, VertexP b, VertexP c) {
		verts.push_back(a);
		verts.push_back(b);
		verts.push_back(c);
	}
	void push_2(VertexP a, VertexP b) {
		verts.push_back(a);
		verts.push_back(b);
	}
	void clear() {
		verts.clear();
	}

	void upload_data();
	void draw_array_static();


	void add_quad(vec2 upper, vec2 lower);

private:
	std::vector<VertexP> verts;
	uint32_t VBO=0;
	uint32_t VAO=0;
	Primitive type = triangle;
	uint32_t allocated_size = 0;
};

enum class FBAttachments
{
	a_none,

	a_depth,

	a_rgba,
	a_rgb,
	a_float16,
	a_float32,
};
struct FramebufferSpec
{
	FramebufferSpec(int width, int height, int samples, FBAttachments attach, bool depth_stencil) :
		width(width), height(height), samples(samples), attach(attach), has_depth_stencil(depth_stencil) {}
	FramebufferSpec() {}

	int width=0, height=0;
	int samples = 1;

	FBAttachments attach = FBAttachments::a_none;
	bool has_depth_stencil = false;
};

struct Framebuffer
{
	uint32_t ID=0;

	FramebufferSpec spec;

	uint32_t color_id=0;
	uint32_t depth_stencil_id=0;
	uint32_t depth_id = 0;

	uint32_t get_color() {
		assert(spec.attach != FBAttachments::a_none);
		return color_id;
	}

	void create(FramebufferSpec n_spec);

	void invalidate();

	void update_window_size(int n_width, int n_height);

	void bind();
	void destroy();
};

void clear_screen(bool color = true, bool depth = true);
void set_clear_color(uint8_t r, uint8_t g, uint8_t b);
void set_sampler2d(uint32_t binding, uint32_t id);

#endif // OPENGL_API_H