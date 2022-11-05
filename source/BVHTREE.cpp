#include "BVHTREE.h"
#include "Map_def.h"
#include <algorithm>

void BrushTree::Build(const worldmodel_t* world, int bstart, int bend)
{
	wm = world;
	brush_start = bstart;
	brush_count = bend - bstart;
	std::vector<Bounds> bounds;
	for (int i = brush_start; i <brush_start+brush_count; i++) {
		const mapbrush_t* mb = &wm->map_brushes[i];
		bounds.push_back(Bounds(mb->min, mb->max));
	}

	wm = world;
	indicies.resize(bounds.size());
	for (int i = 0; i < indicies.size(); i++)
		indicies[i] = i;

	int start = add_new_node();//=0
	build_R(0, bounds.size(), start, bounds);
}

void BrushTree::build_R(int start, int end, int node_num, const std::vector<Bounds>& bounds)
{
	//assert(start >= 0 && end <= bounds.size());
	int num_elements = end - start;
	BVHNode* node = &nodes[node_num];
	node->b = calc_bounds(start, end, bounds);

	if (num_elements <= 1) {
		node->left_node = start;
		node->count = num_elements;
		return;
	}
	int split = start;


	vec3 bounds_center = node->b.get_center();
	int longest_axis = node->b.longest_axis();
	float mid = bounds_center[longest_axis];

	auto split_iter = std::partition(indicies.begin() + start, indicies.begin() + end,
		[longest_axis, mid, bounds](int index) {
			vec3 centroid = (bounds[index].bmax - bounds[index].bmin) * 0.5f;
			return centroid[longest_axis] < mid;
		}
	);
	split = split_iter - indicies.begin();


	if (split == start || split == end)
		split = (start + end) / 2;


	node->count = BVH_BRANCH;
	// These may invalidate the pointer
	int left_child = add_new_node();
	int right_child = add_new_node();
	// So just set it again lol
	nodes[node_num].left_node = left_child;
	build_R(start, split,left_child,bounds);
	build_R(split, end, right_child,bounds);
}
Bounds BrushTree::calc_bounds(int start, int end, const std::vector<Bounds>& bounds) const
{
	Bounds b;
	for (int i = start; i < end; i++)
		b = bounds_union(b,bounds[indicies[i]]);
	return b;
}

bool BrushTree::PointInside(vec3 point) const
{
#if 0
	const vec3 cam_pos = point;
	for (int i = 0; i <wm->map_brushes.size(); i++) {
		const mapbrush_t* brush = &wm->map_brushes[i];
		Bounds b(brush->min, brush->max);
		if (!b.inside(point, 0)) continue;

		bool inside = true;
		for (int j = 0; j < brush->num_faces; j++) {
			plane_t p = wm->brush_sides[brush->face_start + j];
			if (p.dist(cam_pos) > 0) {
				inside = false;
				break;
			}
		}
		if (inside)
			return true;
	}

	return false;
#endif
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
				const mapbrush_t* brush = &wm->map_brushes.at(indicies.at(index_start+i));
				bool inside = true;
				for (int j = 0; j < brush->num_faces; j++) {
					plane_t plane = wm->brush_sides.at(brush->face_start + j);
					if (plane.dist(point) > 0.005) {
						inside = false; 
						break;
					}
				}
				if (inside)
					return true;
			}
			continue;
		}

		bool left_aabb = nodes[node->left_node].b.inside(point,0.01);
		bool right_aabb = nodes[node->left_node + 1].b.inside(point,0.01);

		if (left_aabb) {
			stack[stack_count++] = &nodes[node->left_node];
		}
		if (right_aabb) {
			stack[stack_count++] = &nodes[node->left_node + 1];
		}
	}

	return false;
}
