#ifndef MAP_DEF_H
#define MAP_DEF_H
#include "glm/glm.hpp"
#include <unordered_map>
#include <vector>
#include "Geometry.h"

using namespace glm;
class Texture;
// Parsed from .map file

struct texture_info_t
{
	int t_index=0;
	vec2 uv_scale;
	vec3 axis[2];
	int offset[2];

	int flags = 0;
};

struct mapface_t
{
	int t_info_idx=0;

	int content=0;
	int surf_type=0;

	winding_t wind;

	plane_t plane;
};
#define MAX_FACES 24
// Collection of faces making a convex shape
struct mapbrush_t
{
	int face_start = 0;
	int num_faces = 0;

	int contents = 0;
	int surf_types = 0;

	vec3 min, max;
};

struct entity_t
{
	// Key value pairs for properties
	std::unordered_map<std::string, std::string> properties;
	// associated brush (optional); Only used in parse phase
	int brush_start{}, brush_count{};
};

struct face_t
{
	plane_t plane;
	int v_start, v_count;

	int t_info_idx = 0;

	// lightmapping info
	// these should be hoisted out into a seperate array
	short lightmap_min[2];
	short lightmap_size[2];
	float exact_min[2];
	float exact_span[2];

	bool dont_draw = false;
}; 
// collection of all faces for an entity/collision hull
struct brush_model_t
{
	int face_start=0;
	int face_count=0;

	vec3 min, max;
};

struct LightmapVert
{
	vec3 position;
	vec3 normal;
	vec2 texture_uv;
	vec2 lightmap_uv;
};

struct worldmodel_t
{
	std::vector<brush_model_t> models;	// 0 is world model, rest are indexed by entity key: "model"

	std::vector<vec3> verts;

	std::vector<face_t> faces;

	std::vector<texture_info_t> t_info;

	std::vector<Texture*> textures;

	std::vector<std::string> texture_names;

	std::vector<entity_t> entities;
};


#endif // !MAP_DEF_H
