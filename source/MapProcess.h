#include "MapParser.h"
#include <list>
// Turns parsed map data into faces and verticies
#define MAX_VERTS 16

struct pface_t
{
	plane_t plane;


	int texture_id = 0;
	vec3 texture_axis[2];
	float texture_scale[2];
	u16 texture_offset[2];

	u8 num_verts=0;
	vec3 verts[MAX_VERTS];

	pface_t* next = nullptr;
};

struct pbrush_t
{
	pface_t* faces=nullptr;
	vec3 min, max;
};

struct pbrush_list_t
{
	pbrush_t* start = nullptr;
	vec3 min, max;
};
class MapProcess
{


};