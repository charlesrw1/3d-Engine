#ifndef WORLDGEO_H
#define WORLDGEO_H

#include "MapParser.h"
#include "Model_def.h"
#include "opengl_api.h"

const float SCALE_FACTOR = 1 / 32.f;

#define CONTENTS_EMPTY -1
#define CONTENTS_SOLID -2
#define CONTENTS_CLIP -3

#define SOLID_NOT	 1
#define SOLID_BLOCK	 2
#define SOLID_TRIGGER 3

//80
#define MAX_PER_LEAF 50
#define MAX_DEPTH 10
/*
struct face_t
{
	u32 v_start{};
	u32 v_end{};

	plane_t plane;

	vec3 texture_axis[2];
	float texture_scale[2];
	short texture_offset[2];
	//u8 type;
	//u32 id{};
};*/
struct trace_t
{
	bool hit = false;
	// Rest is undefined if hit=false

	vec3 end_pos;
	// equation of plane hit (a,b,c,d)
	vec3 normal;
	float d;
	
	vec3 dir;
	vec3 start;
	float length;

	int node{};
	// material hit
	Texture* tex = nullptr;
};
struct ray_t
{
	vec3 dir;
	vec3 origin;

	float length = 1000.f;
};



class KDTree
{
public:
	KDTree() {}
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
	enum { CUT_X, CUT_Y, CUT_Z};
	struct node_t {
		// Index into planes
		int plane_num = -1;
		int first_child = -1;	// index into either node array or leaf array

		// If child_count != -1 then node is a leaf
		int num_faces = -1;
		u16 depth=0;	// tree depth
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
		vec3 center;
		int face_index = -1;	// index into faces from world_geo
	};
	std::vector<face_ex_t> face_extended;
	vec3 min = vec3(-128), max = vec3(128);
	
	std::vector<node_t> nodes;
	std::vector<plane_t> planes;
	std::vector<leaf_t> leaves;

	const worldmodel_t* geo = nullptr;

	std::vector<float> work_buffer;

	VertexArray* va=nullptr;
};


class WorldGeometry
{
public:
	WorldGeometry() {}
	void load_map(worldmodel_t* worldmodel);
	void free_map();

	trace_t test_ray(const ray_t& r);

	void debug_draw();
	void create_mesh();

	 Model* get_model()  {
		return model;
	}

	 void print_info() const;
	 // for editor access
	KDTree tree;


	worldmodel_t* wm = nullptr;

	//std::vector<face_t> faces;
	//std::vector<vec3> verts;
	//std::vector<brush_model_t> models;
	//std::vector<texture_info_t> tinfo;
private:
	// Geometry is batched per texture
	std::vector<Texture*> face_textures;

	u32 num_triangles = 0;

	// For now just store the world in a "model"
	Model* model=nullptr;


	// hack for now cause static creation
	VertexArray* line_va;

	VertexArray* hit_faces;
	VertexArray* hit_points;
};

extern WorldGeometry global_world;

#endif // !WORLDGEO_H
