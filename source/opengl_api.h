#ifndef OPENGL_API_H
#define OPENGL_API_H

#include "glm/glm.hpp"
#include "geometry.h"
#include <string>
#include <vector>
using namespace glm;

struct Shader
{
	uint32_t ID=0;

	Shader(const char* vertex_path, const char* fragment_path, const char* geo_path = NULL) {
		load_from_file(vertex_path, fragment_path, geo_path);
	}
	Shader(){}

	void use();
	void load_from_file(const char* vertex_path, const char* fragment_path, const char* geo_path = NULL);
	Shader& set_bool(const char* name, bool value);
	Shader& set_int(const char* name, int value);
	Shader& set_float(const char* name, float value);
	Shader& set_mat4(const char* name, mat4 value);
	Shader& set_vec3(const char* name, vec3 value);
private:
	std::string read_file(const char* filepath);
};

// Vertex primitive, for drawing things like lines, points, quads
struct VertexP
{
	VertexP(vec3 pos, vec3 color) {
		position = pos;
		r = (color.r > 1) ? 255 : color.r * 255;
		g = (color.g > 1) ? 255 : color.g * 255;
		b = (color.b > 1) ? 255 : color.b * 255;
	}
	VertexP(vec3 pos, vec4 color) {
		position = pos;
		r = (color.r > 1) ? 255 : color.r * 255;
		g = (color.g > 1) ? 255 : color.g * 255;
		b = (color.b > 1) ? 255 : color.b * 255;
		a = (color.a > 1) ? 255 : color.a * 255;
	}
	VertexP() {}
	vec3 position = vec3(0);
	vec2 uv = vec2(0);
	uint8_t r=255, g=255, b=255, a=255;
};

// A dynamic array suited for additons/deletions on a smaller scale
struct VertexArray
{
	VertexArray() {}
	~VertexArray();
	using VP = VertexP;

	enum Primitive
	{
		TRIANGLES,
		LINES,
		LINE_STRIP,
		POINTS,
	};
	void init(Primitive type);

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
	void push_square(vec3 p1,vec3 p2, vec3 p3, vec3 p4, vec4 color) {
		push_3(VP(p1, color), VP(p2, color), VP(p3, color));
		push_3(VP(p1, color), VP(p3, color), VP(p4, color));
	}
	void push_line(vec3 v1, vec3 v2, vec3 color) {
		push_2(VP(v1, color), VP(v2, color));
	}
	void clear() {
		verts.clear();
	}
	void add_solid_box(vec3 min, vec3 max, vec4 color)
	{
		vec3 corners[8] = { max, vec3(max.x,max.y,min.z),vec3(min.x,max.y,min.z),vec3(min.x,max.y,max.z),	// top CCW
							vec3(max.x,min.y,max.z), vec3(max.x,min.y,min.z),min,vec3(min.x,min.y,max.z) };	// bottom
		push_square(corners[0], corners[1], corners[2], corners[3], color);// top
		push_square(corners[4], corners[5], corners[6], corners[7], color);// bottom
		push_square(corners[0], corners[1], corners[5], corners[4], color);//+X
		push_square(corners[3], corners[2], corners[6], corners[7], color);//-X
		push_square(corners[3], corners[0], corners[4], corners[7], color);//+Z
		push_square(corners[2], corners[1], corners[5], corners[6], color);//+Z

	}
	void add_line_box(vec3 min, vec3 max, vec3 color)
	{
		using VP = VertexP;
		vec3 corners[8] = { max, vec3(max.x,max.y,min.z),vec3(min.x,max.y,min.z),vec3(min.x,max.y,max.z),	// top CCW
							vec3(max.x,min.y,max.z), vec3(max.x,min.y,min.z),min,vec3(min.x,min.y,max.z) };	// bottom
		for (int i = 0; i < 4; i++) {
			push_2(VP(corners[i], color), VP(corners[(i + 1) % 4], color));
			push_2(VP(corners[4 + i], color), VP(corners[4 + ((i + 1) % 4)], color));
		}

		// connecting
		for (int i = 0; i < 4; i++) {
			push_2(VP(corners[i], color), VP(corners[i + 4], color));
		}

	}

	void upload_data();
	void draw_array_static();


	void add_quad(vec2 upper, vec2 lower);

private:
	std::vector<VertexP> verts;
	uint32_t VBO=0;
	uint32_t VAO=0;
	Primitive type = TRIANGLES;
	uint32_t allocated_size = 0;
};
typedef VertexArray::Primitive VAPrim;

enum class FBAttachments
{
	NONE,

	DEPTH,

	RGBA,
	RGB,
	FLOAT16,
	FLOAT32,
};
struct FramebufferSpec
{
	FramebufferSpec(int width, int height, int samples, FBAttachments attach, bool depth_stencil) :
		width(width), height(height), samples(samples), attach(attach), has_depth_stencil(depth_stencil) {}
	FramebufferSpec() {}

	int width=0, height=0;
	int samples = 1;

	FBAttachments attach = FBAttachments::NONE;
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
		assert(spec.attach != FBAttachments::NONE);
		return color_id;
	}

	void create(FramebufferSpec n_spec);

	void invalidate();

	void resize(int n_width, int n_height);

	void bind();
	void destroy();
};


#endif // OPENGL_API_H