#ifndef BSPTREE_H
#define BSPTREE_H
#include <vector>

#include "MapParser.h"
#include "Geometry.h"
struct ray_t;
struct trace_t;

struct BSPNode
{
	u32 data1=0;
	// For both leafs and interior nodes:
	// bit 31, 0=interior, 1=leaf

	// For interior:
	// bit 0-1, 0=X plane, 1=Y plane, 2=Z plane
	// bit 2-30, offset to first child, child+1 is next child
	// For leaf,
	// bit 0-30, index into face list
	u32 data2=0;
	// For interior:
	// float distance
	// For leaf:
	// u32 child_count
};


class BSPtree
{
public:
	BSPtree() {}
	~BSPtree() {}
	void init(const worldmodel_t* model);
	void create_va();
	void draw();
	void print();

	// Binary searches to find the leaf enclosing the point
	int find_leaf(vec3 point) const;
	// finds leaf and includes the box that surrounds the leaf
	int find_leaf(vec3 point, vec3& min_box, vec3& max_box) const;

	// faster version
	trace_t test_ray(vec3 start, vec3 end);
	// same as above but with debug output
	trace_t test_ray_debug(vec3 start, vec3 end);

	// 10% faster than test_ray
	trace_t test_ray_fast(vec3 start, vec3 end);

private:
	std::vector<BSPNode> fast_list;
	std::vector<u32> face_index;

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
	void subdivide(int node_idx, int depth);

	void create_va_internal(int node_idx, vec3 min, vec3 max);

	inline void check_ray_leaf_node(const node_t& node, const ray_t& r, trace_t& t);
	//inline void check_ray_leaf_node(const node_t& node, vec3& start, vec3& end, trace_t& t);
	inline void check_ray_bsp_node(const BSPNode& leaf, const ray_t& r, trace_t& t);

	void flatten_arrays();
	void make_fast_list();

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

	VertexArray va;
	VertexArray box_array;
};

#endif // !BSPTREE_H
