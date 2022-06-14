#ifndef MAP_PARSER_H
#define MAP_PARSER_H

// A parser for Quake era .map files


#include "glm/glm.hpp"
using namespace glm;

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>

class Texture;

struct plane_t
{
	vec3 normal;
	float d;

	plane_t() {}
	plane_t(const vec3& v1, const vec3& v2, const vec3& v3) {
		init(v1, v2, v3);
	}

	bool operator!=(const plane_t& other);
	void init(const vec3& v1, const vec3& v2, const vec3& v3) {
		normal = cross(v3 - v2, v1 - v2);
		normal = normalize(normal);
		d = -dot(normal, v1);
	}
	float distance(const vec3& v) {
		return dot(normal, v) + d;
	}
};



// Parsed from .map file
struct face_t
{
	vec3 v[3];
	int t_index{};
	vec2 uv_scale;
	vec3 u_axis, v_axis;
	int u_offset, v_offset;

	plane_t plane;
};
#define MAX_FACES 20
struct brush_t
{
	uint16 indices[MAX_FACES];
	uint8 num_i = 0;
};

// Mesh geometry
struct poly_t
{
	short v_start{}, v_end{};
};
struct vert_t
{
	vec3 pos, normal;
	vec2 uv;
};
struct mesh_surface_t
{
	Texture* tex = nullptr;
	std::vector<vert_t> verts;
};

struct parse_entity_t
{
	// Key value pairs for properties
	std::unordered_map<std::string, std::string> properties;
	// Brush (optional), end will be 0 if not used
	u16 brush_start{}, brush_end{};
};

class MapParser
{
public:
	MapParser() {
		parse_buffer.reserve(256);
	}
	void start_file(std::string file);
	const std::vector<poly_t>& get_result();
private:

	enum Result { R_GOOD, R_FAIL, R_EOF };

	void parse_file();

	void compute_intersections(brush_t*);
	void sort_verticies();
	void construct_mesh();

	Result parse_entity();
	Result parse_brush();
	Result parse_face();
	Result parse_vec3(vec3& v);

	Result read_token();
	Result read_str(bool in_quotes=true);

	void fail(const char* msg);

	int get_texture();

	std::vector<brush_t> brushes;	// Groups of faces
	std::vector<face_t> faces;		// Half-face data, 3 verts, texture info
	std::vector<poly_t> polys;		// Groups of verticies, same index as faces


	std::vector<vec3> verts;		// Verticies


	std::vector<std::string> textures;
	std::unordered_map<std::string, int> str_to_tex_index;

	std::vector<parse_entity_t> entities;

	std::ifstream infile;

	std::string file_buffer;
	int file_ptr=0;
	int line = 1;

	std::string parse_buffer;
	char parse_char = 0;
};


#endif