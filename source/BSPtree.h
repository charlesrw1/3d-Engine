#ifndef BSPTREE_H
#define BSPTREE_H
#include <vector>

#include "MapParser.h"
struct ray_t;
struct trace_t;
class BSPtree
{
public:
	BSPtree() {}
	void init(const worldmodel_t* model);
	void create_va();
	void draw() {
		va->draw_array();
	}
	void print();

	void print_trace_stats();
	// Binary searches to find the leaf enclosing the point
	int find_leaf(vec3 point) const;
	// finds leaf and includes the box that surrounds the leaf
	int find_leaf(vec3 point, vec3& min_box, vec3& max_box) const;

	// bad version
	trace_t test_ray(const ray_t& r);

	// faster version
	trace_t test_ray(vec3 start, vec3 end);
private:
	enum { CUT_X, CUT_Y, CUT_Z };
	struct node_t {
		// Index into planes
		int plane_num = -1;
		int first_child = -1;	// index into either node array or leaf array

		// If child_count != -1 then node is a leaf
		int num_faces = -1;
		u16 depth = 0;	// tree depth
		u16 parent = -1;
	};
	// called recursively
	void subdivide(int cut_dir, int node_idx, int depth);

	void create_va_internal(int node_idx, vec3 min, vec3 max);

	void test_ray_internal(bool upstack, int caller_node, int node_idx, const ray_t& r, trace_t& t);
	inline void check_ray_leaf_node(const node_t& node, const ray_t& r, trace_t& t);
	inline void check_ray_leaf_node(const node_t& node, vec3& start, vec3& end, trace_t& t);

	void flatten_arrays();

	struct leaf_t {
		int face_index = -1;	// index into face_ex_t
		int next = -1;	// linked list of leaf_t
	};
	struct face_ex_t {
		vec3 min, max;
		winding_t verts;
		int face_index = -1;	// index into faces from world_geo
	};
	std::vector<face_ex_t> face_extended;
	vec3 min = vec3(-128), max = vec3(128);

	std::vector<node_t> nodes;
	std::vector<plane_t> planes;
	std::vector<leaf_t> leaves;

	const worldmodel_t* geo = nullptr;

	std::vector<float> work_buffer;

	VertexArray* va = nullptr;
};

#endif // !BSPTREE_H
