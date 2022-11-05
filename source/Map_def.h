#ifndef MAP_DEF_H
#define MAP_DEF_H
#include "glm/glm.hpp"
#include <unordered_map>
#include <vector>
#include "Geometry.h"

using namespace glm;
class Texture;
// Parsed from .map file


enum MapSurfFlags
{
	SURF_NODRAW = 1,
	SURF_EMIT = 2,
	SURF_SKYBOX = 4,
};
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
	int model_index;

	vec3 min, max;
};

struct entity_t
{
	// Key value pairs for properties
	std::unordered_map<std::string, std::string> properties;
	// associated brush (optional); Only used in parse phase
	int brush_start{}, brush_count{};


	vec3 get_transformed_origin() const {
		auto org = properties.find("origin");
		if (org == properties.end())
			return vec3(0);
		vec3 res;
		
		sscanf_s(org->second.c_str(), "%f %f %f", &res.x, &res.y, &res.z);
		res /= 32.f;	// scale down

		return vec3(-res.x, res.z, res.y);
	}

	std::string get_classname() const {
		auto classname = properties.find("classname");
		if (classname == properties.end())
			return "NOCLASSNAME";
		return classname->second;
	}
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

struct AmbientCube
{
	vec3 position;
	vec3 axis_colors[6];	// x,-x,y,...
};

struct EnviormentProbe
{
	vec3 position;
	int cubemap_index=-1;
	bool dont_draw = false;
};

struct worldmodel_t
{
	std::string name;
	int lightmap_width, lightmap_height;

	std::vector<brush_model_t> models;	// 0 is world model, rest are indexed by entity key: "model"
	std::vector<vec3> verts;
	std::vector<face_t> faces;
	std::vector<texture_info_t> t_info;
	std::vector<Texture*> textures;
	std::vector<std::string> texture_names;
	std::vector<entity_t> entities;
	 
	/* Unmodified brush data for determining inside/outside */
	std::vector<plane_t> brush_sides;
	std::vector<mapbrush_t> map_brushes;

	std::vector<AmbientCube> ambient_grid;
};


#endif // !MAP_DEF_H
