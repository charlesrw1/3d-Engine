#ifndef MAP_PARSER_H
#define MAP_PARSER_H

// A parser for Quake era .map files

#define QUAKE_FROMAT 1

#include "glm/glm.hpp"
using namespace glm;

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>

class Texture;
class VertexArray;

enum SideType { FRONT, BACK, ON_PLANE, SPLIT };
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
	float distance(const vec3& v) const {
		return dot(normal, v) + d;
	}
	// true= in front or on the plane, false= behind
	bool classify(const vec3& p) const {
		return dist(p) > -0.001;
	}
	float dist(const vec3& p) const {
		return dot(normal,p) + d;
	}
	SideType classify_full(const vec3& p) const {
		float d = dist(p);
		if (d < -0.01f) {
			return BACK;
		}
		else if (d > 0.01f) {
			return FRONT;
		}
		else {
			return ON_PLANE;
		}
	}
	bool get_intersection(const vec3& start, const vec3& end, vec3& result, float& percentage) const
	{
		vec3 dir = end - start;
		dir = normalize(dir);

		float denom = dot(normal, dir);
		if (fabs(denom) < 0.01) {
			return false;
		}
		float dis = distance(start);
		float t = -dis / denom;
		result = start + dir * t;

		percentage = t / length(end - start);
		
		return true;
	}


};

// Parsed from .map file
struct mface_t
{
	int t_index{};
	vec2 uv_scale;
	vec3 u_axis, v_axis;
	int u_offset, v_offset;

	int v_start{}, v_end{};
	plane_t plane;
};
#define MAX_FACES 24
// Collection of faces
struct mbrush_t
{
	int face_start = 0;
	uint8 num_faces = 0;
};

struct mentity_t
{
	// Key value pairs for properties
	std::unordered_map<std::string, std::string> properties;
	// associated brush (optional)
	int brush_start{}, brush_count{};
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
	
	void CSG_union();
	void clip_to_brush(mbrush_t& to_clip, const mbrush_t& b, bool clip_to_plane, std::vector<mface_t>& final_faces);
	std::vector<mface_t> clip_to_list(const mbrush_t& a, int start_index, const mface_t& b, bool clip_to_plane);
	void split_face(const mface_t& b, const mface_t& a, mface_t& front, mface_t& back);
	mface_t copy_face(const mface_t& f1);

	uint64 get_surface_area();

	void make_clipping_hull();

	SideType classify_face(const mface_t& face, const mface_t& other) const;

	Result parse_entity();
	Result parse_brush();


	Result parse_face();
	Result parse_face_quake();

	Result parse_vec3(vec3& v);

	Result read_token();
	Result read_str(bool in_quotes=true);

	void parse_fail(const char* msg);
	
	int get_texture();

	std::vector<mbrush_t> brushes;	// Groups of faces
	std::vector<mface_t> faces;	
	std::vector<vec3> verts;		// Indexed into by faces


	// Used as temporary buffers during CSG stage
	std::vector<vec3> clipped_verts;
	std::vector<mbrush_t> clipped_brushes;
	std::vector<mface_t> clipped_faces;


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