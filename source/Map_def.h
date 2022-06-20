#ifndef MAP_DEF_H
#define MAP_DEF_H
#include "glm/glm.hpp"
#include <unordered_map>
#include <vector>

using namespace glm;

enum side_t { FRONT, BACK, ON_PLANE, SPLIT };
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
		return dot(normal, p) + d;
	}
	side_t classify_full(const vec3& p) const {
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
#define MAX_VERTS 24
struct winding_t
{
	int num_verts=0;
	vec3 v[MAX_VERTS];

	void add_vert(vec3 vert) {
		assert(num_verts < MAX_VERTS);
		v[num_verts++] = vert;
	}
};
// Parsed from .map file

struct texture_info_t
{
	int t_index=0;
	vec2 uv_scale;
	vec3 u_axis, v_axis;
	int u_offset, v_offset;

	vec3 world_to_tex;
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

struct mapentity_t
{
	// Key value pairs for properties
	std::unordered_map<std::string, std::string> properties;
	// associated brush (optional)
	int brush_start{}, brush_count{};
};

struct face_t
{
	plane_t plane;
	int v_start, v_count;

	int t_info_idx = 0;

	short lightmap_min[2];
	short lightmap_size[2];
};
// collection of all faces for an entity/collision hull
struct brush_model_t
{
	int face_start=0;
	int face_count=0;

	vec3 min, max;
};

struct generic_lightmapped_vert_t
{
	vec3 position;
	vec3 normal;
	vec2 texture_uv;
	vec2 lightmap_uv;
};


#endif // !MAP_DEF_H
