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
class VertexArray;

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
	// true= in front or on the plane, false= behind
	bool classify(const vec3& p) {
		return dist(p) > -0.001;
	}
	float dist(const vec3& p) {
		return dot(normal,p) + d;
	}
};

// Parsed from .map file
struct mface_t
{
	vec3 v[3];

	int t_index{};
	vec2 uv_scale;
	vec3 u_axis, v_axis;
	int u_offset, v_offset;

	int v_start{}, v_end{};
	plane_t plane;
};
#define MAX_FACES 20
// Collection of faces
struct mbrush_t
{
	uint16 indices[MAX_FACES];
	uint8 num_i = 0;
};

struct mentity_t
{
	// Key value pairs for properties
	std::unordered_map<std::string, std::string> properties;
	// associated brush (optional)
	u16 brush_start{}, brush_end{};
};

class MapParser
{
public:
	MapParser() {
		parse_buffer.reserve(256);
	}
	void start_file(std::string file);
	void construct_mesh(VertexArray& va, VertexArray& edges);


	const std::vector<vec3>& get_verts() const {
		return verts;
	}
	const std::vector<mface_t>& get_brush_faces() const {
		return faces;
	}
	const std::vector<std::string>& get_textures() const {
		return textures;
	}
	const std::vector<mentity_t>& get_entities() const {
		return entities;
	}
private:

	enum Result { R_GOOD, R_FAIL, R_EOF };

	void parse_file();

	void compute_intersections(mbrush_t*);
	void sort_verticies(mface_t*);

	Result parse_entity();
	Result parse_brush();


	Result parse_face();
	Result parse_face_quake();

	Result parse_vec3(vec3& v);

	Result read_token();
	Result read_str(bool in_quotes=true);

	void fail(const char* msg);

	int get_texture();

	std::vector<mbrush_t> brushes;	// Groups of faces
	std::vector<mface_t> faces;	


	std::vector<vec3> verts;		// Indexed into by faces


	std::vector<std::string> textures;
	std::unordered_map<std::string, int> str_to_tex_index;

	std::vector<mentity_t> entities;

	std::ifstream infile;

	std::string file_buffer;
	int file_ptr=0;
	int line = 1;

	std::string parse_buffer;
	char parse_char = 0;
};


#endif