#ifndef LIGHT_H
#define LIGHT_H

#include "geometry.h"
struct worldmodel_t;
void create_light_map(worldmodel_t* wm);
void draw_lightmap_debug();


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

	vec3 reflectance;

	patch_t* next = nullptr;
};


int light_main(int argc, char** argv);

#endif // !LIGHT_H



