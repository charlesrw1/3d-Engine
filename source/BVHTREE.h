#pragma once
#include <glm/glm.hpp>
#include <vector>
using glm::vec3;
struct Bounds
{
	Bounds() : bmin(INFINITY), bmax(-INFINITY) {}
	explicit Bounds(vec3 pos) : bmin(pos), bmax(pos) {}
	Bounds(vec3 min, vec3 max) : bmin(min), bmax(max) {}

	float surface_area() const {
		vec3 size = bmax - bmin;
		return 2.0 * (size.x * size.y + size.x * size.z + size.y * size.z);
	}

	bool inside(vec3 p, float size) const {
		return bmin.x <= p.x + size && bmax.x >= p.x - size &&
			bmin.y <= p.y + size && bmax.y >= p.y - size &&
			bmin.z <= p.z + size && bmax.z >= p.z - size;
	}
	bool inside(vec3 p) const {
		return bmin.x <= p.x  && bmax.x >= p.x  &&
			bmin.y <= p.y  && bmax.y >= p.y &&
			bmin.z <= p.z && bmax.z >= p.z;
	}
	bool intersect(const Bounds& other) const {
		return bmin.x <= other.bmax.x && bmax.x >= other.bmin.x &&
			bmin.y <= other.bmax.y && bmax.y >= other.bmin.y &&
			bmin.z <= other.bmax.z && bmax.x >= other.bmin.z;
	}

	vec3 get_center() const {
		return (bmin + bmax) / 2.f;
	}
	int longest_axis() const {
		vec3 lengths = bmax - bmin;
		int max_num = 0;
		if (lengths[1] > lengths[max_num])
			max_num = 1;
		if (lengths[2] > lengths[max_num])
			max_num = 2;
		return max_num;
	}

	vec3 bmin, bmax;
};
inline Bounds bounds_union(const Bounds& b1, const Bounds& b2) {
	Bounds b;
	b.bmin = glm::min(b1.bmin, b2.bmin);
	b.bmax = glm::max(b1.bmax, b2.bmax);
	return b;
}
inline Bounds bounds_union(const Bounds& b1, const vec3& v) {
	Bounds b;
	b.bmin = glm::min(b1.bmin, v);
	b.bmax = glm::max(b1.bmax, v);
	return b;
}
inline bool bounds_intersect(const Bounds& b1, const Bounds& b2)
{
	return (b1.bmin.x < b2.bmax.x&& b2.bmin.x < b1.bmax.x)
		&& (b1.bmin.y < b2.bmax.y&& b2.bmin.y < b1.bmax.y)
		&& (b1.bmin.z < b2.bmax.z&& b2.bmin.z < b1.bmax.z);
}
struct worldmodel_t;
class BrushTree
{
public:
	static const int BVH_BRANCH = -1;

	struct BVHNode {
		Bounds b;

		int left_node;
		int count;
	};
	bool PointInside(vec3 point) const;
	void Build(const worldmodel_t* wm, int bstart,int bend);
	void build_R(int start, int end, int node_num, const std::vector<Bounds>& bounds);
	Bounds calc_bounds(int start, int end, const std::vector<Bounds>& bounds) const;
	int add_new_node() {
		nodes.resize(nodes.size() + 1);
		return nodes.size() - 1;
	}
	std::vector<BVHNode> nodes;
	std::vector<int> indicies;

	int brush_start{};
	int brush_count{};
	const worldmodel_t* wm{};
};