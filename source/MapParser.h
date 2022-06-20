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

#include "Map_def.h"

class Texture;
class VertexArray;



class MapParser
{
public:
	MapParser() {
		parse_buffer.reserve(256);
	}
	void start_file(std::string file);
	void construct_mesh(VertexArray& va, VertexArray& edges);

	// Final data
	std::vector<mapentity_t> entities;
	std::vector<texture_info_t> t_info;	// holds axis info and index into 'texture' strings
	std::vector<std::string> textures;
	std::vector<face_t> face_list;
	std::vector<brush_model_t> model_list;
	std::vector<vec3> vertex_list;
private:

	enum Result { R_GOOD, R_FAIL, R_EOF };

	void parse_file();

	void compute_intersections(mapbrush_t*);
	void sort_verticies(mapface_t*);
	
	void CSG_union();
	void clip_to_brush(mapbrush_t& to_clip, const mapbrush_t& b, bool clip_to_plane, std::vector<mapface_t>& final_faces);
	std::vector<mapface_t> clip_to_list(const mapbrush_t& a, int start_index, const mapface_t& b, bool clip_to_plane);
	void split_face(const mapface_t& b, const mapface_t& a, mapface_t& front, mapface_t& back);

	uint64 get_surface_area();

	void make_clipping_hull();

	side_t classify_face(const mapface_t& face, const mapface_t& other) const;

	Result parse_entity();
	Result parse_brush();


	Result parse_face();
	Result parse_face_quake();

	Result parse_vec3(vec3& v);

	Result read_token();
	Result read_str(bool in_quotes=true);

	void parse_fail(const char* msg);
	
	int get_texture();


	std::vector<mapbrush_t> brushes;	// Groups of faces
	std::vector<mapface_t> faces;	
	//std::vector<vec3> verts;		// Indexed into by faces


	// Used as temporary buffers during CSG stage
	//std::vector<vec3> clipped_verts;
	//std::vector<mbrush_t> clipped_brushes;
	//std::vector<mface_t> clipped_faces;

	std::unordered_map<std::string, int> str_to_tex_index;

	std::ifstream infile;

	std::string file_buffer;
	int file_ptr=0;
	int line = 1;

	std::string parse_buffer;
	char parse_char = 0;
};


#endif