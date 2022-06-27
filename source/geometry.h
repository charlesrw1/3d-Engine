#ifndef GEOMETRY_H
#define GEOMETRY_H
#include "glm/glm.hpp"

using vec3 = glm::vec3;
using vec4 = glm::vec4;

using mat4 = glm::mat4;


#define MAX_WINDING_VERTS 24
enum side_t { FRONT, BACK, ON_PLANE, SPLIT };
struct plane_t
{
	vec3 normal;
	float d;

	plane_t() {}
	plane_t(const vec3& v1, const vec3& v2, const vec3& v3) {
		init(v1, v2, v3);
	}

	//bool operator!=(const plane_t& other);
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
		return dist(p) > -0.01;
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

struct winding_t
{
	int num_verts = 0;
	vec3 v[MAX_WINDING_VERTS];

	void add_vert(vec3 vert) {
		assert(num_verts < MAX_WINDING_VERTS);
		v[num_verts++] = vert;
	}
	vec3 get_center() const;
	float get_area() const;
};
void split_winding(const winding_t& a, const plane_t& plane, winding_t& front, winding_t& back);
void get_extents(const winding_t& w, vec3& min, vec3& max);
void sort_winding(winding_t& w);

// classify face 1 against the plane
side_t classify_face(const winding_t& f1, const plane_t& p);

struct ray_t
{
	vec3 dir=vec3(1,0,0);
	vec3 origin=vec3(0);

	float length = 1000.f;
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



#endif