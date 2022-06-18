#ifndef GEOMETRY_H
#define GEOMETRY_H
#include "glm/glm.hpp"

const float MAX_RAY_DIST = 2000.f;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

using mat4 = glm::mat4;


struct Ray
{
	vec3 direction = vec3(1,0,0);
	vec3 origin = vec3(0);
	float length = MAX_RAY_DIST;
};
struct Plane
{
	vec3 normal;
	float offset;
};
struct AABB
{
	vec3 max=vec3(0.f);
	vec3 min=vec3(0.f);

	bool intersects(AABB& other)  {
		return min.x <= other.max.x && max.x >= other.min.x &&
			min.y <= other.max.y && max.y >= other.min.y &&
			min.z <= other.max.z && max.z >= other.min.z;
	}
	bool contains(vec3 p)  {
		return min.x <= p.x && max.x >= p.x &&
			min.y <= p.y && max.y >= p.y &&
			min.z <= p.z && max.z >= p.z;
	}
	AABB get_object_aabb(mat4 matrix)  {
		AABB res = *this;
		res.max = matrix * vec4(res.max, 1.0);
		res.min = matrix * vec4(res.min, 1.0);
		return res;
	}

	void create_from_points(vec3* points, unsigned num_points);
};
using u16 = uint16_t;
struct sector_t
{
	u16 id{};
	
	// Collection of planes defining sector
	u16 s_start{};
	u16 s_end{};




};


#endif