#ifndef LIGHT_H
#define LIGHT_H
#include "geometry.h"

struct LightmapSettings
{
	float patch_grid = 2.f;
	float pixel_density = 4.f;
	int num_bounces = 64;
	bool enable_radiosity = true;
	bool inside_map = true;
	bool test_patch_visibility = true;
	bool no_direct = false;
	float sample_ofs = 0.1;


	vec3 default_reflectivity = vec3(0.4);
};


struct worldmodel_t;
void create_light_map(worldmodel_t* wm, LightmapSettings settings);
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
	transfer_t* transfers = nullptr;
	int num_transfers = 0;

	vec3 center;
	int face;
	float area = 0;

	vec3 reflectivity=vec3(0.9);

	vec3 sample_light = vec3(0);
	int num_samples = 0;

	vec3 total_light = vec3(0);

	patch_t* next = nullptr;
};


int light_main(int argc, char** argv);

#endif // !LIGHT_H



