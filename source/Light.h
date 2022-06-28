#ifndef LIGHT_H
#define LIGHT_H

#include "geometry.h"
struct worldmodel_t;
void create_light_map(worldmodel_t* wm);
void draw_lightmap_debug();
void draw_lightmap_patches();

enum class PatchDebugMode
{
	RANDOM,
	LIGHTCOLOR,
	NUMTRANSFERS,
	AREA,
	RADCOLOR,
};
void set_patch_debug(PatchDebugMode mode);

// Radiosity of i = Emitted of i + Reflectance of i * sum[Form factor of i to j * Radiosity of j]
struct transfer_t
{
	// 0-1 of how much light is transfered from the owning patch to "patch_num", sum of all transfers is 1.0
	float factor = 0;
	unsigned int patch_num=0;
};

struct patch_t
{
	winding_t winding;
	transfer_t* transfers=nullptr;	// array of all patches 
	int num_transfers = 0;

	vec3 center;
	int face;
	float area = 0;

	vec3 reflectance=vec3(0.9);

	vec3 sample_light = vec3(0);
	int num_samples = 0;

	vec3 total_light = vec3(0);

	patch_t* next = nullptr;
};

struct facelight_t
{
	int num_points = 0;
	int point_offset = 0;
};
#define MAX_NEIGHBORS 8
struct neighbor_faces_t
{
	int neighbors[MAX_NEIGHBORS];
	int num_neighbors;
};



int light_main(int argc, char** argv);

#endif // !LIGHT_H



