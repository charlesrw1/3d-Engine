#ifndef WORLDGEO_H
#define WORLDGEO_H

#include "MapParser.h"
#include "Model_def.h"
#include "opengl_api.h"

const float SCALE_FACTOR = 1 / 32.f;

enum SurfaceType : u8
{
	SF_SOLID,
	SF_CLIP,
	SF_TRIGGER,
	SF_WATER,
};
struct face_t
{
	u32 v_start{};
	u32 v_end{};

	plane_t plane;

	u8 type;
};
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
	void load_map(const MapParser& mp);
	void free_map();

	trace_t test_ray(const ray_t& r);

	void debug_draw();

	 Model* get_model()  {
		return model;
	}

	 void print_info() const;
private:
	// All verticies used in the world, accessed by faces
	std::vector<vec3> world_verts;

	// Static world geometry, drawn every frame
	std::vector<face_t> static_faces;

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
