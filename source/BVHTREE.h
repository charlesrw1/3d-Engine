#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <algorithm>
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

struct SimplePoint
{
	vec3 point;
	vec3 GetPoint() const {
		return point;
	}
};

template< typename POINTTYPE >
class PointTree
{
public:
	static const int BVH_BRANCH = -1;

	struct BVHNode {
		Bounds b;

		int left_node;
		int count;
	};
	// returned array is index into points
	void PointsInRadius(vec3 point, float radius, std::vector<int>& indicies) const;
	void Build();
	void build_R(int start, int end, int node_num);
	Bounds calc_bounds(int start, int end) const;
	int add_new_node() {
		nodes.resize(nodes.size() + 1);
		return nodes.size() - 1;
	}
	std::vector<BVHNode> nodes;
	std::vector<POINTTYPE> points;
};
template< typename POINTTYPE >
void PointTree<POINTTYPE>::Build()
{
	int start = add_new_node(); // = 0
	build_R(0, points.size(), start);
}
template< typename POINTTYPE >
void PointTree<POINTTYPE>::build_R(int start, int end, int node_num)
{
	//assert(start >= 0 && end <= bounds.size());
	int num_elements = end - start;
	BVHNode* node = &nodes[node_num];
	node->b = calc_bounds(start, end);

	if (num_elements <= 1) {
		node->left_node = start;
		node->count = num_elements;
		return;
	}
	int split = start;


	vec3 bounds_center = node->b.get_center();
	int longest_axis = node->b.longest_axis();
	float mid = bounds_center[longest_axis];

	auto split_iter = std::partition(points.begin() + start, points.begin() + end,
		[longest_axis, mid](const POINTTYPE& point) {
			return point.GetPoint()[longest_axis] < mid;
		}
	);
	split = split_iter - points.begin();


	if (split == start || split == end)
		split = (start + end) / 2;


	node->count = BVH_BRANCH;
	// These may invalidate the pointer
	int left_child = add_new_node();
	int right_child = add_new_node();
	// So just set it again lol
	nodes[node_num].left_node = left_child;
	build_R(start, split, left_child);
	build_R(split, end, right_child);
}
template< typename POINTTYPE >
Bounds PointTree<POINTTYPE>::calc_bounds(int start, int end) const
{
	Bounds b;
	for (int i = start; i < end; i++) {
		b = bounds_union(b, points[i].GetPoint());
	}
	return b;
}
template< typename POINTTYPE >
void PointTree<POINTTYPE>::PointsInRadius(vec3 center_point, float radius, std::vector<int>& indicies) const
{
	float radius_2 = radius * radius;
	indicies.clear();
	const BVHNode* stack[64];
	stack[0] = &nodes[0];

	int stack_count = 1;
	const BVHNode* node = nullptr;

	float dist_to_closest = INFINITY;

	while (stack_count > 0)
	{
		node = stack[--stack_count];

		if (node->count != BVH_BRANCH)
		{
			int index_count = node->count;
			int index_start = node->left_node;
			for (int i = 0; i < index_count; i++) {
				const POINTTYPE* point = &points.at(index_start + i);
				float dist_2 = dot(center_point - point->GetPoint(), center_point - point->GetPoint());
				if (dist_2 <= radius_2)
					indicies.push_back(index_start + i);
			}
			continue;
		}

		bool left_aabb = nodes[node->left_node].b.inside(center_point,radius);
		bool right_aabb = nodes[node->left_node + 1].b.inside(center_point,radius);

		if (left_aabb) {
			stack[stack_count++] = &nodes[node->left_node];
		}
		if (right_aabb) {
			stack[stack_count++] = &nodes[node->left_node + 1];
		}
	}
}
