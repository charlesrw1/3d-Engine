#ifndef WORLDGEO_H
#define WORLDGEO_H

#include "MapParser.h"
#include "Model_def.h"
#include "opengl_api.h"
#include "BSPtree.h"

const float SCALE_FACTOR = 1 / 32.f;

#define CONTENTS_EMPTY -1
#define CONTENTS_SOLID -2
#define CONTENTS_CLIP -3

#define SOLID_NOT	 1
#define SOLID_BLOCK	 2
#define SOLID_TRIGGER 3

//80
#define MAX_PER_LEAF 10
#define MAX_DEPTH 64
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
	int face{};
	// material hit
	Texture* tex = nullptr;
};
struct ray_t
{
	vec3 dir;
	vec3 origin;

	float length = 1000.f;
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
	BSPtree tree;


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
