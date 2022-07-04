#ifndef WORLDGEO_H
#define WORLDGEO_H

#include "MapParser.h"
#include "Model_def.h"
#include "opengl_api.h"
#include "BSPtree.h"


#define MAX_PER_LEAF 10
#define MAX_DEPTH 64

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
	int face{};
	// material hit
	Texture* tex = nullptr;
};


class WorldGeometry
{
public:
	WorldGeometry() {}
	void load_map(worldmodel_t* worldmodel);
	void free_map();

	trace_t brute_force_raycast(const ray_t& r);

	void draw_trace_hits();

	void draw_face_edges();

	void create_mesh();

	 Model* get_model()  {
		return model;
	}
	 void set_worldmodel(worldmodel_t* worldmodel) {
		 wm = worldmodel;
	 }
	 void init_render_data();
	 void upload_map_to_tree();

	 void print_info() const;
	BSPtree tree;


	worldmodel_t* wm = nullptr;

private:
	// Geometry is batched per texture
	std::vector<Texture*> face_textures;

	u32 num_triangles = 0;

	// For now just store the world in a "model"
	Model* model=nullptr;

	VertexArray line_va;

	VertexArray hit_faces;
	VertexArray hit_points;
};

extern WorldGeometry global_world;

#endif // !WORLDGEO_H
