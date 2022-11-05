#ifndef LIGHT_H
#define LIGHT_H
#include "geometry.h"
#include <vector>


struct LightmapSettings
{
	int samples_per_patch = 100;

	// how many texels per 1.0 meters/units
	// 32 quake units = 1 my units
	float pixel_density = 4.f;
	// Determines size of a patch, more patches dramatically slow radiosity lighting time (its O(n^2))
	float patch_grid = 2.f;
	// Number of bounce iterations, you already pay the price computing form factors so might as well set it high
	int num_bounces = 64;

	bool enable_radiosity = true;
	// Determines if "outside" faces are removed with a simple raycast 
	bool inside_map = true;
	bool test_patch_visibility = true;
	bool no_direct = false;
	float sample_ofs = 0.05;

	vec3 default_reflectivity = vec3(0.3);

	// Dirt = ambient occlusion
	float dirt_dist = 8.0;	// dist=raycast length
	float dirt_scale = 1.0;	// scale=scales occlusion
	float dirt_gain = 1.0;	// gain= exponential multiplier
	int num_dirt_vectors = 25;	// number of raycasts for calculating dirt
	bool using_dirt = false;


	/* Debugging */
	bool debug_dirt = false;
	bool debug_indirect_occluded_samples = true;
};
extern LightmapSettings config;




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
	VARIANCE,
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

	float sky_visibility = 0.0;

	vec3 center;
	int face;
	float area = 0;

	vec3 reflectivity=vec3(0.9);

	bool occluded = false;
	vec3 sample_light = vec3(0);
	int num_samples = 0;

	vec3 total_light = vec3(0);
	vec3 emission = vec3(0.0);

	int self_index;
	int next=-1;// index into patches
	bool is_sky = false;
};


enum light_type_t
{
	LIGHT_POINT,
	LIGHT_SURFACE,
	LIGHT_SPOT,
	LIGHT_SUN,
};
struct light_t
{
	vec3 pos;
	vec3 color;
	vec3 normal;	// sun/spot direction
	float width;	// spot cone width

	light_type_t type;

	float brightness;
	int face_idx;	// area lights
};

struct Skylight
{
	bool has_sun = false;
	vec3 sun_dir=vec3(0,-1,0);
	vec3 sky_color=vec3(0);
};

/* LIGHT.cpp */
struct BrushTree;
extern const BrushTree* GetBrushTree();
extern const worldmodel_t* GetWorld();
extern vec3 CalcDirectLightingAtPoint(vec3 point, vec3 normal);
extern const LightmapSettings* GetConfig();

/* LIGHTENT.cpp */
extern void AddLightEntities(worldmodel_t* world);
extern std::vector<light_t>& LightList();
extern Skylight& GetSky();

 vec3 RGBToFloat(unsigned char r, unsigned char b, unsigned char g);


 /* PATCH.cpp */
 extern std::vector<patch_t> patches;
 extern void MakePatches();

 /* LMBUFFER.cpp */

 class Lightmap
 {

 };


#endif // !LIGHT_H



