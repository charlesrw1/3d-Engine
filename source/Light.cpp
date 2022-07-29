#include "Light.h"
#include "WorldGeometry.h"
#include "SDL2/SDL_timer.h"
#include "ThreadTools.h"
#include <algorithm>
#include "Voxelizer.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "Sampling.h"

const worldmodel_t* world;

face_t* faces;
int num_faces;

const vec3* verts;
int num_verts;

texture_info_t* tinfo;
int num_tinfo;
#define MAX_PATCHES 0x10000

patch_t patches[MAX_PATCHES];
int num_patches = 0;

patch_t* face_patches[MAX_PATCHES];

std::string* tex_strings;
int num_tex_strings;

// ---------- LIGHTING ------------
struct facelight_t
{
	int num_points = 0;
	vec3* sample_points = nullptr;

	int num_pixels = 0;
	vec3* pixel_colors = nullptr;

	int width=0, height = 0;
};

facelight_t facelights[0x10000];

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

std::vector<light_t> lights;
static int env_light_index = -1;
vec3 sky_color = vec3(0);// vec3(0.5, 0.7, 1.0);

// --------------------------------



// ------ RADIOSITY ARRAYS -------
vec3 radiosity[MAX_PATCHES];	// excindent
// -------------------------------

// -------- SETTINGS --------------
// how many texels per 1.0 meters/units
// 32 quake units = 1 my units
// this is how many actual pixels are in the lightmap, each pixel gets supersampled with 4 additional samples 
float density_per_unit = 10.f;
// Determines size of a patch, more patches dramatically slow radiosity lighting time (its O(n^2))
float patch_grid = 0.5f;
// Number of bounce iterations, you already pay the price computing form factors so might as well set it high
int num_bounces = 64;
// False means direct only
bool enable_radiosity = true;
// determine if "outward facing" faces are culled and not computed, helps performance and file space for Quake and indoor maps
bool inside_map = true;
// Cast a ray from the patch to the center of a face to prevent light bleeds, breaks on some maps
bool test_patch_visibility = true;
// This is sort of hacky, it should be taken from the actual face textures themselves
vec3 default_reflectivity = vec3(0.3);
// Dont add direct lighting to final lightmap, still gets computed for radiosity patch reflectivity 
bool no_direct = false;

// Use random vectors vs exact for patch-to-patch form factor
#define USE_RANDOM_VECTORS
 int NUM_PATCH_RAYCASTS=100;
// ---------------------------------

// ---------- DEBUG ---------------
VertexArray va;
VertexArray point_array;
VertexArray voxels;
VertexArray patch_va;
static PatchDebugMode cur_mode;
// ---------------------------------


// -------- COPLANAR DATA ----------
struct planeneighbor_t
{
	planeneighbor_t* next = nullptr;
	int face = 0;
};
static int num_planes = 0;
planeneighbor_t planes[MAX_PATCHES];
int index_to_plane_list[MAX_PATCHES];
// ----------------------------------


// ------- AMBIENT CUBE -------------
struct AmbCubeTransfer
{
	struct CubeSide {
		vec3 total_light = vec3(0.0);

	}sides[6];

	bool inside_wall = false;
};
std::vector<vec3> ambient_cubes;
std::vector<AmbCubeTransfer> amb_cube_transfers;
// -----------------------------------

// ------------ UTILITIES --------------

vec3 lerp(const vec3& a, const vec3& b, float percentage)
{
	return a + (b - a) * percentage;
}

vec3 rgb_to_float(uint8 r, uint g, uint b) {
	return vec3(r / 255.f, g / 255.f, b / 255.f);
}
vec3 random_color()
{
	vec3 v;
	for (int i = 0; i < 3; i++) {
		v[i] = rand() / (RAND_MAX+1.f);
	}
	return v;
}
// --------------------------------------

struct Rectangle
{
	Rectangle(int x1, int y1, int x2, int y2)
	{
		x = x1;
		y = y1;
		width = x2 - x1;
		height = y2 - y1;
	}
	Rectangle() {}
	int x, y;
	int width, height;
};

struct PackNode
{
	~PackNode() {
		delete child[0];
		delete child[1];
	}
	PackNode* child[2] = { nullptr,nullptr };
	Rectangle rc;
	int image_id = -1;
};



struct Image
{
	int width, height;
	int buffer_start;

	int face_num;
};

// ---------- LIGHTMAP IMAGE --------
PackNode* root = nullptr;
std::vector<Image> images;
std::vector<u8> data_buffer;
std::vector<u8> final_lightmap;
int lm_width = 1200;
int lm_height = 1200;
// ----------------------------------

bool vec3_compare(vec3& v1, vec3& v2, float epsilon)
{
	return fabs(v1.x - v2.x) < epsilon && fabs(v1.y - v2.y) < epsilon && fabs(v1.z - v2.z) < epsilon;
}


// Assumes face is rectangular
vec3 random_point_on_rectangular_face(int face_idx)
{
	face_t* f = faces + face_idx;
	vec3 V0 = verts[f->v_start] - verts[f->v_start + 1];
	vec3 V1 = verts[f->v_start + 1] - verts[f->v_start + 2];

	float u = random_float();
	float v = random_float();

	return verts[f->v_start + 2] + u * V0 + v * V1;
}


 float RADIAL = patch_grid*4.f;
 float RADIALSQRT = sqrt(RADIAL);

// This is similar to what Valve uses in VRAD, Quake 2's triangulation method was garbage frankly
class Radial
{
public:
	Radial(facelight_t* fl, face_t* face) : fl(fl), face(face) {
		weights = new float[fl->num_pixels];
		memset(weights, 0, fl->num_pixels*sizeof(float));
		ambient_light = new vec3[fl->num_pixels];
		memset(ambient_light, 0, fl->num_pixels * sizeof(vec3));
	}
	~Radial() {
		delete[] weights;
		delete[] ambient_light;
	}
	void build_ambient_samples(int face_num) {
		patch_t* p;
		planeneighbor_t* pn;
		
		// Get face extents and middle
		winding_t face_winding;
		vec3 face_middle=vec3(0.0f);
		vec3 face_min, face_max;
		for (int i = 0; i < face->v_count; i++) {
			face_winding.add_vert(verts[face->v_start + i]);
			face_middle += verts[face->v_start + i];
		}
		face_middle /= face->v_count;
		get_extents(face_winding, face_min, face_max);

		// Loop through all patches on the plane that the face is on
		pn = &planes[index_to_plane_list[face_num]];
		while (pn)
		{
			p = face_patches[pn->face];
			while (p)
			{
				vec3 patch_min, patch_max;
				get_extents(p->winding, patch_min, patch_max);
				int i;
				for (i = 0; i < 3; i++) {
					// if patch is roughly inside the face bounds, keep it
					if (patch_max[i]+patch_grid<face_min[i])
						break;
					if (patch_min[i]-patch_grid>face_max[i])
						break;
				}
				if (i == 3) {
					// prevents some light bleeding, some make it through though
					trace_t check_wall_collison;
					check_wall_collison.hit = false;
					// vec3 compare fixes cases where center and face middle are the same, if so, dont cast a ray
					if (test_patch_visibility && !vec3_compare(p->center, face_middle, 0.01f)) {
						check_wall_collison = global_world.tree.test_ray_fast(p->center, face_middle);
					}
					if (!test_patch_visibility || !check_wall_collison.hit) {
						add_ambient_sample(p, patch_min, patch_max);

					}
				}
				p = p->next;
			}
			pn = pn->next;
		}


	}
	// patch is roughly on the face, check all the points and how much the patch contributes to it
	void add_ambient_sample(patch_t* p, vec3 min, vec3 max) {

		vec3 size = max - min;
		vec3 ext_min = min - size * vec3(RADIALSQRT);
		vec3 ext_max = max + size * vec3(RADIALSQRT);
		vec3 patch_center = p->center;
		for (int i = 0; i < fl->num_points; i++) {
			vec3 point = fl->sample_points[i];
			int j;
			for (j = 0; j < 3; j++) {
				if (point[j]<ext_min[j]-0.1f || point[j]>ext_max[j]+0.1f)
					break;
			}
			// point is outside the bounds of patches "influence"
			if (j != 3)
				continue;

			float sq_dist = dot(patch_center - point, patch_center - point);
			float weight = RADIAL - sq_dist;
			if (weight < 0)
				continue;
			weights[i] += weight;
			ambient_light[i] += p->total_light * weight;
		}
	}
	vec3 get_ambient(int sample_num) {
		if (weights[sample_num] > 0) {
			return ambient_light[sample_num] / weights[sample_num];
		}
		return vec3(1, 0, 1);
	}

private:
	face_t* face = nullptr;
	facelight_t* fl = nullptr;
	float* weights = nullptr;
	vec3* ambient_light = nullptr;
};


void make_space(int pixels)
{
	data_buffer.resize(data_buffer.size() + pixels * 3, 0);
}
void add_color(int start, int offset, vec3 color)
{
	for (int i = 0; i < 3; i++) {
		int add_val = color[i] * 255;
		if (add_val < 0)add_val = 0;
		int cur_val = data_buffer.at(start + (offset * 3) + i);
		int add = cur_val + add_val;
		if (add > 255) add = 255;

		data_buffer.at(start + (offset * 3) + i) = add;

	}
}

void patch_for_face(int face_num)
{
	const face_t* face = &faces[face_num];
	texture_info_t* ti = tinfo + face->t_info_idx;
	std::string* t_string = tex_strings + ti->t_index;
	// Skybox doesn't make patches
	if (face->dont_draw || ti->flags & SURF_EMIT) {
		return;
	}
	patch_t* p = &patches[num_patches++];
	assert(num_patches < 0x10000);
	p->face = face_num;
	face_patches[face_num] = p;

	for (int i = 0; i < face->v_count; i++) {
		p->winding.add_vert(verts[face->v_start + i]);
	}
	p->area = p->winding.get_area();
	p->area = max(p->area, 0.1f);
	p->center = p->winding.get_center();


	if (*t_string == "color/red") {
		p->reflectivity = rgb_to_float(237, 28, 36);
	}
	else if (*t_string == "color/green") {
		p->reflectivity = rgb_to_float(34,177,76);
	}
	else if (*t_string == "color/blue") {
		p->reflectivity = rgb_to_float(0, 128, 255);
	}
	else if (*t_string == "color/yellow") {
		p->reflectivity = rgb_to_float(255, 242, 0);
	}
	else {
		auto rflc_idx = t_string->find("dev/rflc");
		if (rflc_idx != std::string::npos) {
			int brightness = std::stoi(t_string->substr(t_string->size() - 2, 2));
			p->reflectivity = vec3(brightness/100.0);
		}
		else {
			p->reflectivity = default_reflectivity;
		}
	}

	p->next = nullptr;
}

void make_patches()
{
	const brush_model_t* bm = &world->models[0];
	for (int i = bm->face_start; i < bm->face_start + bm->face_count; i++) {
		patch_for_face(i);
	}
}

static int failed_subdivide = 0;
void subdivide_patch(patch_t* p)
{
	vec3 min, max;
	winding_t front, back;
	get_extents(p->winding, min, max);
	int i;
	for (i = 0; i < 3; i++) {
		// Quake 2 had the patch grid as an actual grid thats not relative to a specific patch
		// However this leads to some really small patches, more matches no matter the size tank performance (n^2)
		// Instead make it relative to the face
		if (min[i]+patch_grid+(patch_grid) <= max[i]) {
			plane_t split;
			split.normal = vec3(0);
			split.normal[i] = 1.f;
			split.d = -(min[i]+ patch_grid);
			bool res = try_split_winding(p->winding, split, front, back);
			if (res) {
				break;
			}
			failed_subdivide++;
		}
	}
	if (i == 3) {
		return;
	}

	assert(num_patches < MAX_PATCHES);
	patch_t* new_p = &patches[num_patches++];

	new_p->next = p->next;
	p->next = new_p;

	p->winding = front;
	new_p->winding = back;

	new_p->face = p->face;

	p->center = p->winding.get_center();
	new_p->center = new_p->winding.get_center();
	p->area = p->winding.get_area();
	new_p->area = new_p->winding.get_area();

	new_p->reflectivity = p->reflectivity;

	new_p->sample_light = p->sample_light;
	new_p->total_light = p->total_light;
	new_p->emission = p->emission;


	subdivide_patch(p);
	subdivide_patch(new_p);

}
void subdivide_patches()
{
	int num = num_patches;
	for (int i = 0; i < num; i++) {
		subdivide_patch(&patches[i]);
	}
	printf("Failed num: %d\n", failed_subdivide);
}


void create_patch_view(PatchDebugMode mode)
{
	using VP = VertexP;
	using PDM = PatchDebugMode;
	patch_va.init(VAPrim::TRIANGLES);
	for (int i = 0; i < num_patches; i++) {
		patch_t* p = &patches[i];
		winding_t* w = &p->winding;
		vec3 color;
		switch (mode)
		{
		case PDM::RANDOM:
			color = random_color();
			break;
		case PDM::AREA:
			color = vec3(p->area / (patch_grid*patch_grid*5.f));
			break;
		case PDM::LIGHTCOLOR:
			color = p->sample_light;
			break;
		case PDM::NUMTRANSFERS:
			color = vec3(p->num_transfers * 2.f / (num_patches));
			break;
		case PDM::RADCOLOR:
			color = p->total_light;
			break;
		}
		for (int i = 0; i < w->num_verts-2; i++) {
		//	patch_va.push_line(w->v[i], w->v[(i + 1)%w->num_verts], vec3(1.0));
			patch_va.push_3(VP(w->v[0], color), VP(w->v[i + 1], color), VP(w->v[(i + 2) % w->num_verts],color));
		}
	}
}
void set_patch_debug(PatchDebugMode mode)
{
	if (cur_mode == mode)
		return;
	cur_mode = mode;
	patch_va.clear();
	create_patch_view(mode);
}

// RADIOSITY FUNCTIONS
//std::vector<float> transfers;	// work buffer

struct TempTransfer
{
	int patch_idx;
	float transfer_amt;
};

void make_transfers(int patch_num)
{
	patch_t* patch = &patches[patch_num];
	plane_t plane = faces[patch->face].plane;
	const int i = patch_num;
	float total = 0;
	vec3 center_with_offset = patch->center + plane.normal * 0.01f;
	patch->num_transfers = 0;

	std::vector<int> patch_hits;
	for (int i = 0; i < NUM_PATCH_RAYCASTS; i++) 
	{
		vec3 direction = sample_hemisphere_cos(plane.normal, i, NUM_PATCH_RAYCASTS - 1);

		trace_t res = global_world.tree.test_ray_fast(center_with_offset, center_with_offset + direction * 300.f,-0.005,0.005);
		if (!res.hit || tinfo[faces[res.face].t_info_idx].flags & SURF_SKYBOX) {
			patch_hits.push_back(-1);	// Sky index
			continue;
		}

		// Find patch that was hit
		patch_t* p = face_patches[res.face];
		while (p)
		{
			if (p->winding.point_inside(res.end_pos)) break;
			p = p->next;
		}
		if (p == nullptr) {
			//printf("Face hit but patch not found\n");
			continue;
		}
		patch_t* other = p;
		int patch_idx = (long long(other) - (long long)&patches[0]) / sizeof(patch_t);
		if (patch_idx == patch_num) {
			continue;
		}
		patch_hits.push_back(patch_idx);
	}

	std::sort(patch_hits.begin(), patch_hits.end());
	
	patch->transfers = new transfer_t[NUM_PATCH_RAYCASTS];
	patch->num_transfers = 0;
	for (int i = 0; i < patch_hits.size(); i++) {
		int index = patch_hits[i];
		int start = i;
		while (i < patch_hits.size() && patch_hits[i] == index)
		{
			i++;
		}
		int count = i - start;
		float factor = count / (float)NUM_PATCH_RAYCASTS;
		if (index == -1) {
			patch->sky_visibility = factor;
		}
		else {
			patch->transfers[patch->num_transfers].factor = factor;
			patch->transfers[patch->num_transfers].patch_num = index;
			patch->num_transfers++;
		}
	}

}


struct PatchHitAndDir
{
	int patch_hit;
	vec3 direction;
};

void sample_ambient_cubes(int cube_num)
{
	vec3 point = ambient_cubes[cube_num];
	AmbCubeTransfer* act = &amb_cube_transfers[cube_num];

	std::vector<PatchHitAndDir> patch_hits;
	int backface_hits = 0;
	for (int i = 0; i < NUM_PATCH_RAYCASTS; i++)
	{
		vec3 direction = sample_hemisphere_uniform(vec3(0, 1, 0), i, NUM_PATCH_RAYCASTS);
		for (int j = 0; j < 2; j++)
		{
			// sample full sphere
			direction = (j == 0) ? direction : -direction;
			trace_t res = global_world.tree.test_ray_fast(point, point + direction * 300.f, -0.005, 0.005);
			if (!res.hit || tinfo[faces[res.face].t_info_idx].flags & SURF_SKYBOX) {
				patch_hits.push_back({ -1,direction });	// Sky index
				continue;
			}
			if (res.hit_backface()) {
				backface_hits++;
				if (backface_hits >= 15) {
					act->inside_wall = true;
					return;
				}
				continue;
			}

			// Find patch that was hit
			patch_t* p = face_patches[res.face];
			while (p)
			{
				if (p->winding.point_inside(res.end_pos)) break;
				p = p->next;
			}
			if (p == nullptr) {
				//printf("Face hit but patch not found\n");
				continue;
			}
			patch_t* other = p;
			int patch_idx = (long long(other) - (long long)&patches[0]) / sizeof(patch_t);
			patch_hits.push_back({ patch_idx,direction });
		}
	}

	for (int i = 0; i < 6; i++)
	{
		float total = 0.0;

		AmbCubeTransfer::CubeSide* cs = &act->sides[i];
		cs->total_light = vec3(0.0);
		vec3 side_normal = vec3(0);
		int dig = i / 2;
		side_normal[dig] = (i & 1) ? -1.0 : 1.0;

		for (int j = 0; j < patch_hits.size(); j++)
		{
			float cos_theta = dot(patch_hits[j].direction, side_normal);
			if (cos_theta <= 0)
				continue;

			total += cos_theta;
			if (patch_hits[j].patch_hit == -1) {
				cs->total_light += sky_color * cos_theta;
				continue;
			}

			patch_t* ph = &patches[patch_hits[j].patch_hit];
			cs->total_light += ((ph->total_light + ph->sample_light) * ph->reflectivity) * cos_theta;
		}

		cs->total_light /= total;
	}

}



void start_radiosity()
{
	for (int i = 0; i < num_patches; i++) {
		patch_t* p = &patches[i];
		radiosity[i] = p->sample_light * p->reflectivity + p->emission;
	}
	for (int i = 0; i < num_bounces; i++)
	{
		for (int j = 0; j < num_patches; j++)
		{
			vec3 total = vec3(0.0);
			patch_t* p = &patches[j];
			total += p->sky_visibility * sky_color;

			for (int t = 0; t < p->num_transfers; t++)
			{
				transfer_t* tran = p->transfers + t;
				total += radiosity[tran->patch_num] * tran->factor;
			}
			// Incoming irradiance
			p->total_light = total;
		}
		// Collect light
		for (int j = 0; j < num_patches; j++)
		{
			patch_t* p = &patches[j];

			radiosity[j] = (p->total_light + p->sample_light) * p->reflectivity + p->emission;
		}

	}

}


void free_patches()
{
	for (int i = 0; i < num_patches; i++) {
		delete[] patches[i].transfers;
	}
	amb_cube_transfers.clear();
}


const int MAX_POINTS = 256 * 256;
struct LightmapState
{
	vec3 face_middle = vec3(0);

	vec3 tex_normal;
	vec3 tex_origin = vec3(0);

	vec3 world_to_tex[2];	// u = dot(world_to_tex[0], world_pos-origin)
	vec3 tex_to_world[2];	// world_pos = tex_origin + u * tex_to_world[0]

	int numpts = 0;

	int tex_min[2];
	int tex_size[2];
	float exact_min[2];
	float exact_max[2];

	facelight_t* fl=nullptr;

	// For supersampling, final facelight_t points are the center of the 4 (they might get shifted around)
	std::vector<vec3> sample_points[4];
	int num_samples = 4;

	int surf_num = 0;
	face_t* face = nullptr;
};

static u32 total_pixels = 0;
static u32 total_rays_cast = 0;
static int num_unique_planes = 0;
bool plane_equals(plane_t& p1, plane_t& p2)
{
	return (fabs(p1.d - p2.d) < 0.001f) 
		&& (fabs(p1.normal.x - p2.normal.x) < 0.001f) 
		&& (fabs(p1.normal.y - p2.normal.y) < 0.001f) 
		&& (fabs(p1.normal.z - p2.normal.z) < 0.001f);
}
void find_coplanar_faces()
{
	printf("Finding coplanar faces...\n");
	memset(index_to_plane_list, -1, sizeof(index_to_plane_list));
	for (int i = 0; i < num_faces; i++) {
		if (index_to_plane_list[i] != -1)
			continue;
		face_t* f = &faces[i];
		planeneighbor_t* p = &planes[num_planes];
		planeneighbor_t* last_p = p;
		p->face = i;
		int plane_index = num_planes;
		index_to_plane_list[i] = plane_index;
		num_planes++;
		num_unique_planes++;
		for (int j = i+1; j < num_faces; j++) {
			if (index_to_plane_list[j] != -1)
				continue;
			face_t* of = &faces[j];

			if (!plane_equals(f->plane, of->plane))
				continue;
			
			planeneighbor_t* opn = &planes[num_planes++];

			last_p->next = opn;
			last_p = opn;
			opn->face = j;

			index_to_plane_list[j] = plane_index;
		}
	}
	printf("Num unique planes: %d\n", num_unique_planes);
}


void add_sample_to_patch(vec3 point, vec3 color, int face_num)
{
	patch_t* patch = face_patches[face_num];
	vec3 min, max;

	while (patch)
	{
		get_extents(patch->winding, min, max);

		for (int i = 0; i < 3; i++) {
			if (min[i] > point[i] + 0.25) {
				goto skip_patch;
			}
			if (max[i] < point[i] - 0.25) {
				goto skip_patch;
			}
		}
		patch->sample_light += color;
		patch->num_samples++;

	skip_patch:;
		patch = patch->next;
	}
}

// Lots of help from Quake's LTFACE.c 

void calc_extents(LightmapState& l)
{
	texture_info_t* ti = &tinfo[l.face->t_info_idx];
	// Calc face min and max
	float min[2], max[2];
	min[0] = min[1] = 50000;
	max[0] = max[1] = -50000;
	int vcount = l.face->v_count;
	l.face_middle = vec3(0);
	for (int i = 0; i < vcount; i++) {

		l.face_middle += verts[l.face->v_start + i];


		for (int j = 0; j < 2; j++) {

			float val = dot(verts[l.face->v_start + i], ti->axis[j]);
			if (val < min[j]) {
				min[j] = val;
			}
			if (val > max[j]) {
				max[j] = val;
			}

		}
	}
	l.face_middle /= vcount;

	for (int i = 0; i < 2; i++) {
		l.tex_min[i] = min[i];
		l.tex_size[i] = ceil(max[i]) - floor(min[i]);
		if (l.tex_size[i] > 64) {
			l.tex_size[i] = 64;
			printf("Texture size too big!\n");
			//exit(1);
		}
		l.exact_min[i] = min[i];
		l.exact_max[i] = max[i];

		l.face->exact_min[i] = min[i];
		l.face->exact_span[i] = max[i] - min[i];
	}
}
const float of_dist = 0.1f;
float sample_ofs[4][2] = { {1,1},{1,-1},{-1,-1},{-1,1} };

void calc_points(LightmapState& l, Image& img)
{
	int h = l.tex_size[1] * density_per_unit + 1;
	int w = l.tex_size[0] * density_per_unit + 1;
	float start_u = l.exact_min[0];	
	float start_v = l.exact_min[1];
	l.numpts = h * w;
	if (l.numpts > MAX_POINTS) {
		l.numpts = MAX_POINTS;
	}
	total_pixels += l.numpts;

	l.fl->height = h;
	l.fl->width = w;

	l.fl->num_pixels = l.numpts;
	l.fl->num_points = l.numpts;

	for (int i = 0; i < l.num_samples; i++) {
		l.sample_points[i].resize(l.numpts);
	}

	img.height = h;
	img.width = w;

	float step[2];
	step[0] = (l.exact_max[0] - l.exact_min[0]) / (w-1);
	step[1] = (l.exact_max[1] - l.exact_min[1]) / (h-1);

	const face_t* f = l.face;
	winding_t wind;
	for (int i = 0; i < f->v_count; i++) {
		wind.add_vert(verts[f->v_start + i]);
	}

	l.fl->pixel_colors = new vec3[l.numpts];
	l.fl->sample_points = new vec3[l.numpts];

	vec3 face_mid = l.face_middle + l.face->plane.normal * 0.01f;

	for (int s = 0; s < l.num_samples; s++) {
		int top_point = 0;
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				float ofs[2];
				ofs[0] = (l.num_samples == 1) ? 0 : sample_ofs[s][0];
				ofs[1] = (l.num_samples == 1) ? 0 : sample_ofs[s][1];

				float u = start_u + (x)*step[0] + ofs[0];
				float v = start_v + (y)*step[1] + ofs[1];

				vec3 point = l.tex_origin + l.tex_to_world[0] * u + l.tex_to_world[1] * v + l.face->plane.normal * 0.01f;


				// is the point outside the bounds?
				//		cast ray towards middle
				//		did it hit?
				//			no, point is good
				//			yes, find closest point on face winding, move sample to that position

				if (!wind.point_inside(point)) {
					auto trace_res = global_world.tree.test_ray_fast(point, face_mid);
					if (trace_res.hit) {
						point = wind.closest_point_on_winding(point);
					}
				}

				if (top_point >= MAX_POINTS) {
					l.numpts = MAX_POINTS - 1;
					img.height = y - 1;
					l.fl->height = y - 1;
					goto face_too_big;
				}
				assert(top_point < MAX_POINTS);
				l.sample_points[s][top_point++] = point;
			}
		}
	face_too_big:;
	}
}

void calc_vectors(LightmapState& l)
{
	assert(l.face->t_info_idx >= 0 && l.face->t_info_idx < num_tinfo);
	texture_info_t* ti = &tinfo[l.face->t_info_idx];


	// face vectors
	l.world_to_tex[0] = ti->axis[0];
	l.world_to_tex[1] = ti->axis[1];

	l.tex_normal = normalize(cross(ti->axis[1], ti->axis[0]));

	float ratio = dot(l.tex_normal, l.face->plane.normal);
	if (ratio < 0) {
		ratio = -ratio;
		l.tex_normal = -l.tex_normal;
	}

	for (int i = 0; i < 2; i++) {
		float dist = dot(l.world_to_tex[i], l.face->plane.normal);
		dist /= ratio;
		l.tex_to_world[i] = l.world_to_tex[i] + l.tex_normal * -dist;
	}
	l.tex_origin = vec3(0.f);
	float dist = -l.face->plane.d;
	dist /= ratio;
	l.tex_origin += l.tex_normal * dist;
}

void light_face(int num)
{
	LightmapState l;
	Image img;
	assert(num < num_faces);
	l.face = &faces[num];
	l.surf_num = num;
	l.fl = &facelights[num];
	img.face_num = num;

	texture_info_t* ti = tinfo + l.face->t_info_idx;
	if (l.face->dont_draw)
		return;

	calc_vectors(l);
	calc_extents(l);

	calc_points(l, img);

	for (int i = 0; i < l.numpts; i++) {
		l.fl->pixel_colors[i] = vec3(0);
	}
	// calc middle points
	for (int i = 0; i < l.numpts; i++) {
		vec3 total = vec3(0.f);
		for (int s = 0; s < l.num_samples; s++) {
			total += l.sample_points[s][i];
		}
		l.fl->sample_points[i] = total / (float)l.num_samples;
	}

	float sample_scale = 1 / (float)l.num_samples;
	for (int j = 0; j < l.numpts; j++) {
		for (int i = 0; i < lights.size(); i++) {
			for (int s = 0; s < l.num_samples; s++) {
				light_t* light = &lights[i];
				vec3 sample_pos = l.sample_points[s][j];
				vec3 final_color = vec3(0.0);

				switch (light->type) {
				case LIGHT_SPOT:
				case LIGHT_POINT:
				{
					vec3 light_p = lights[i].pos;
					float plane_dist = l.face->plane.distance(light_p);
					if (plane_dist < 0) {
						continue;
					}
					float sqrd_dist = dot(light_p - sample_pos, light_p - sample_pos);
					if (sqrd_dist > lights[i].brightness * lights[i].brightness) {
						continue;
					}
					total_rays_cast++;
					trace_t res = global_world.tree.test_ray_fast(sample_pos, light_p, -0.005f, 0.005f);
					if (res.hit) {
						continue;
					}

					vec3 light_dir = light_p - sample_pos;
					float dist = length(light_dir);
					light_dir = normalize(light_dir);
					float dif = dot(light_dir, l.face->plane.normal);
					if (dif < 0) {
						continue;
					}

					float extra_scale = 1.f;
					if (light->type == LIGHT_SPOT) {
						float cos_theta = dot(-light_dir, light->normal);
						float outer = light->width * 0.90;
						float epsilon = light->width - outer;
						extra_scale = clamp((cos_theta - outer) / epsilon, 0.0f, 1.0f);
					}

					if (lights[i].type == LIGHT_SURFACE) {
						extra_scale = -dot(lights[i].normal, light_dir);
						extra_scale = glm::max(extra_scale, 0.f);
					}
					float attenuation_factor = ((float)lights[i].brightness / (dist * dist + 0.01)) *(1 - dist/(float)lights[i].brightness);
					//max(1 / (dist * dist + lights[i].brightness) * (lights[i].brightness - dist), 0.f)
					final_color = lights[i].color * dif * attenuation_factor * extra_scale;
				} break;
				case LIGHT_SURFACE:
				{
					const face_t* lface = faces + light->face_idx;
					if (!lface->plane.classify(sample_pos))
						continue;
					// Load verts into winding...
					winding_t w;
					for (int v = 0; v < lface->v_count; v++) {
						w.add_vert(verts[lface->v_start + v]);
					}
					vec3 closest = w.closest_point_on_winding(sample_pos);
					float distsq = dot(closest - sample_pos, closest - sample_pos);
					if (distsq > light->brightness * light->brightness)
						continue;
					vec3 closest_dir = normalize(closest - sample_pos);
					if (dot(closest_dir, l.face->plane.normal) <= 0)
						continue;

					float area = w.get_area();
					int num_surf_samples = ceil(area / (patch_grid * patch_grid));	// arbitrary
					
					
					vec3 accumulate = vec3(0.0);
					for (int surf_samp = 0; surf_samp < num_surf_samples; surf_samp++) {
						vec3 point;
						vec3 dir;
						if (surf_samp == 0) {
							point = closest;
							dir = closest_dir;
						}
						else {
							point = random_point_on_rectangular_face(light->face_idx);
							dir = normalize(point - sample_pos);
						}

						point += lface->plane.normal * 0.01f;

						trace_t res = global_world.tree.test_ray_fast(sample_pos, point, -0.005f, 0.005f);
						if (res.hit) {
							continue;
						}


						float dist = length(point - sample_pos);

						float dif = dot(dir, l.face->plane.normal);
						float attenuation_factor = ((float)lights[i].brightness / (dist * dist + 0.01)) * (1 - dist / (float)lights[i].brightness);
						accumulate += lights[i].color * dif * attenuation_factor;
					}
					accumulate /= (float)num_surf_samples;
					final_color = accumulate;
				}break;
				case LIGHT_SUN:
				{
					float angle = dot(l.face->plane.normal, -light->normal);
					if (angle <= 0)
						continue;
					total_rays_cast++;
					trace_t res = global_world.tree.test_ray_fast(sample_pos, sample_pos + (-light->normal * 300.f));
					// If it traces to the sun, it should hit a skybox face
					if (res.hit) {
						face_t* face = faces + res.face;
						texture_info_t* ti = tinfo + face->t_info_idx;
						if (!(ti->flags & SURF_SKYBOX))
							continue;
					}
					final_color = light->color * angle * (float)light->brightness;
				} break;
				default:
					break;
				}

				l.fl->pixel_colors[j] += final_color*sample_scale;
			}
		}
		if (enable_radiosity) {
			add_sample_to_patch(l.fl->sample_points[j], l.fl->pixel_colors[j], num);
		}
	}
	if (enable_radiosity) {
		patch_t* patch = face_patches[num];
		while (patch)
		{
			if (patch->num_samples > 0) {
				patch->sample_light /= patch->num_samples;
			}
			patch = patch->next;
		}
	}

}
void final_light_face(int face_num)
{
	face_t* face = &faces[face_num];
	facelight_t* fl = &facelights[face_num];
	texture_info_t* ti = tinfo + face->t_info_idx;
	if (face->dont_draw)
		return;
	Image img;
	img.face_num = face_num;
	img.height = fl->height;
	img.width = fl->width;
	img.buffer_start = data_buffer.size();
	make_space(img.height * img.width);
	
	if (enable_radiosity) {
		Radial radial(fl,face);
		radial.build_ambient_samples(face_num);
		for (int i = 0; i < fl->num_points; i++) {
			if (no_direct) {
				fl->pixel_colors[i] = vec3(0.0);
			}
			
			fl->pixel_colors[i] += radial.get_ambient(i);
		}
	}
	
	for (int y = 0; y < img.height; y++) {
		for (int x = 0; x < img.width; x++) {
			vec3 total = vec3(0);
			int added = 0;
			int offset = y * img.width + x;
			total = fl->pixel_colors[y * img.width + x];
			total = glm::pow(total, vec3(1.0 / 2.2));
			add_color(img.buffer_start, offset, total);
		}
	}

	images.push_back(img);


	delete[] fl->pixel_colors;
	delete[] fl->sample_points;
}


PackNode* insert_imgage(PackNode* pn, const Image* img)
{
	if (pn->child[0] != nullptr) {
		PackNode* new_n = insert_imgage(pn->child[0], img);
		if (new_n != nullptr) {
			return new_n;
		}
		return insert_imgage(pn->child[1], img);
	}
	else {
		if (pn->image_id != -1)
			return nullptr;
		if (pn->rc.height < img->height || pn->rc.width < img->width)
			return nullptr;
		if (pn->rc.height == img->height && pn->rc.width == img->width)
			return pn;
		pn->child[0] = new PackNode;
		pn->child[1] = new PackNode;
		int dw = pn->rc.width - img->width;
		int dh = pn->rc.height - img->height;
		if (dw > dh) {

			pn->child[0]->rc = Rectangle(pn->rc.x, pn->rc.y, pn->rc.x + img->width, pn->rc.y + pn->rc.height);
			pn->child[1]->rc = Rectangle(pn->rc.x + img->width, pn->rc.y, pn->rc.x + pn->rc.width, pn->rc.y + pn->rc.height);
		}
		else {
			pn->child[0]->rc = Rectangle(pn->rc.x, pn->rc.y, pn->rc.x + pn->rc.width, pn->rc.y + img->height);
			pn->child[1]->rc = Rectangle(pn->rc.x, pn->rc.y + img->height, pn->rc.x + pn->rc.width, pn->rc.y + pn->rc.height);
		}
		return insert_imgage(pn->child[0], img);
	}
}
void add_to_buffer(int x, int y, u8 r, u8 g, u8 b)
{
	assert(x < lm_width&& y < lm_height);
	int start = y * lm_width * 3 + x * 3;
	final_lightmap.at(start) = r;
	final_lightmap.at(start + 1) = g;
	final_lightmap.at(start + 2) = b;
}


void append_to_lightmap(const PackNode* pn, const Image* img)
{
	int x_coord = pn->rc.x;
	int y_coord = pn->rc.y;

	for (int y = 0; y < img->height; y++) {
		for (int x = 0; x <img->width; x++) {

			int temp_buf_offset = img->buffer_start + y * img->width * 3 + x * 3;
			u8 r, g, b;
			r = data_buffer.at(temp_buf_offset);
			g = data_buffer.at(temp_buf_offset + 1);
			b = data_buffer.at(temp_buf_offset + 2);

			add_to_buffer(x_coord + x, y_coord + y, r, g, b);
		}
	}

}

void add_lights(worldmodel_t* wm)
{
	std::string work_str;
	work_str.reserve(64);
	for (int i = 0; i < wm->entities.size(); i++) {
		entity_t* ent = &wm->entities.at(i);
		auto find = ent->properties.find("classname");
		vec3 color = vec3(1.0);
		float brightness = 200;
		light_t l;
		
		if (find->second == "light_torch_small_walltorch") {
			color = vec3(1.0, 0.3, 0.0);
			brightness = 100;
			l.type = LIGHT_POINT;

			//continue;
		}
		else if (find->second == "light_flame_large_yellow") {
			color = vec3(1.0, 0.5, 0.0);
			brightness = 400;
			l.type = LIGHT_POINT;

			//continue;
		}
		else if (find->second == "light") {
			l.type = LIGHT_POINT;
		}
		else if (find->second == "light_fluoro") {
			color = vec3(0.8, 0.8, 1.0);
			l.type = LIGHT_POINT;

		}
		else if (find->second == "spot_light") {
			l.type = LIGHT_SPOT;
		}
		else {
			continue;
		}

		work_str = ent->properties.find("origin")->second;
		vec3 org;
		sscanf_s(work_str.c_str(), "%f %f %f", &org.x, &org.y, &org.z);
		org /= 32.f;	// scale down

		org = vec3(-org.x, org.z, org.y);

		auto brightness_str = ent->properties.find("light");
		if (brightness_str != ent->properties.end()) {
			work_str = brightness_str->second;
			brightness = std::stoi(work_str);
		}
		brightness /= 32.f;

		// "color" isn't in Quake's light entities, but it is in my own format
		auto color_str = ent->properties.find("color");
		if (color_str != ent->properties.end()) {
			work_str = ent->properties.find("color")->second;
			sscanf_s(work_str.c_str(), "%f %f %f", &color.r, &color.g, &color.b);
		}
		l.pos = org;
		l.color = color;
		l.brightness = brightness;
		
		if (l.type == LIGHT_SPOT) {
			float width = std::stof(ent->properties.find("width")->second);
			width = cos(radians(width));
			l.width = width;

			std::string angle = ent->properties.find("mangle")->second;
			int angles[3];
			sscanf_s(angle.c_str(), "%d %d %d", &angles[0], &angles[1], &angles[2]);

			vec3 dir = vec3(0);
			dir.x = cos(radians((float)angles[1])) * cos((radians((float)angles[0])));
			dir.y = sin((radians((float)angles[0])));
			dir.z = -sin(radians((float)angles[1])) * cos(radians((float)angles[0]));

			l.normal = -dir;
		}

		lights.push_back(l);
	}
	for (int i = 0; i < num_faces; i++)
	{
		const face_t* face = &faces[i];
		texture_info_t* ti = tinfo + face->t_info_idx;
		if (!(ti->flags & SURF_EMIT)) {
			continue;
		}

		light_t l;
		// brightness value is stored in the x texture scale
		l.brightness = ti->uv_scale[0];
		l.color = vec3(1.f);
		l.type = LIGHT_SURFACE;
		l.normal = face->plane.normal;
		vec3 center = vec3(0);
		for (int j = 0; j < face->v_count; j++) {
			center += verts[face->v_start + j];
		}
		center /= (float)face->v_count;
		l.face_idx = i;

		lights.push_back(l);
	}

	for (const auto& e : world->entities)
	{
		if (e.get_classname() != "enviorment_light")
			continue;

		std::string angle = e.properties.find("mangle")->second;
		int angles[3];
		sscanf_s(angle.c_str(), "%d %d %d", &angles[0], &angles[1], &angles[2]);

		vec3 sun_direction = vec3(0);
		sun_direction.x = cos(radians((float)angles[1])) * cos((radians((float)angles[0])));
		sun_direction.y = sin((radians((float)angles[0])));
		sun_direction.z = -sin(radians((float)angles[1])) * cos(radians((float)angles[0]));

		light_t l;
		l.type = LIGHT_SUN;
		l.brightness = std::stoi(e.properties.find("brightness")->second);
		l.color = rgb_to_float(253, 243, 208);
		l.normal = -normalize(sun_direction);
		
		env_light_index = lights.size();
		
		lights.push_back(l);
		break;
	}


}
void mark_bad_faces()
{
	const brush_model_t* bm = &world->models[0];
	for (int i = bm->face_start; i < bm->face_start + bm->face_count; i++) {
		face_t* f = &faces[i];
		vec3 middle = vec3(0.f);
		for (int v = 0; v < f->v_count; v++) {
			middle += verts[f->v_start+v];
		}
		middle /= f->v_count;
		middle += f->plane.normal * 0.02f;

		// A hacky way to check if a face is "outside" the playarea. 
		// There are some false positives, but no false negatives so it works alright
		total_rays_cast++;
		trace_t check_outside_world = global_world.tree.test_ray_fast(middle, middle + f->plane.normal * 200.f);
		if (!check_outside_world.hit) {
			f->dont_draw = true;
		}

	}
}

void ambient_voxel_debug()
{
	for (int i = 0; i < amb_cube_transfers.size(); i++)
	{
		vec3 point = ambient_cubes.at(i);
		auto& act = amb_cube_transfers.at(i);
		if (act.inside_wall)
			continue;
		vec3 colors[6];
		colors[0] = act.sides[0].total_light;// x
		colors[1] = act.sides[1].total_light;// -x
		colors[2] = act.sides[2].total_light;// y
		colors[3] = act.sides[3].total_light;// -y
		colors[4] = act.sides[4].total_light;// z
		colors[5] = act.sides[5].total_light;// -

		voxels.add_solid_box(point - vec3(0.25), point + vec3(0.25), colors);
	}
}

void add_manual_ambient_cubes()
{
	for (const auto& ent : world->entities) {
		auto classname = ent.properties.find("classname");
		if (classname == ent.properties.end())
			continue;
		if (classname->second != "AmbientCube")
			continue;

		vec3 origin = ent.get_transformed_origin();

		ambient_cubes.push_back(origin);
	}
}


void create_light_map(worldmodel_t* wm, LightmapSettings settings)
{
	patch_grid = settings.patch_grid;
	density_per_unit = settings.pixel_density;
	enable_radiosity = settings.enable_radiosity;
	test_patch_visibility = settings.test_patch_visibility;
	inside_map = settings.inside_map;
	num_bounces = settings.num_bounces;
	default_reflectivity = settings.default_reflectivity;
	no_direct = settings.no_direct;
	RADIAL = patch_grid * 4.f;
	RADIALSQRT = sqrt(RADIAL);
	NUM_PATCH_RAYCASTS = settings.samples_per_patch;

	sky_color = rgb_to_float(124, 138, 203);
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 2; j++) {
			sample_ofs[i][j] *= settings.sample_ofs;
		}
	}

	world = wm;
	u32 start = SDL_GetTicks();
	printf("Starting lightmap...\n");
	va.init(VertexArray::Primitive::LINES);
	point_array.init(VertexArray::Primitive::POINTS);
	voxels.init(VertexArray::TRIANGLES);

	// Used for random debug colors
	srand(time(NULL));

	Voxelizer vox(wm, 1.0,2);
	vox.move_final_samples(ambient_cubes);
	add_manual_ambient_cubes();
	amb_cube_transfers.resize(ambient_cubes.size());



	faces = wm->faces.data();
	num_faces = wm->faces.size();

	verts = wm->verts.data();
	num_verts = wm->verts.size();

	tinfo = wm->t_info.data();
	num_tinfo = wm->t_info.size();

	tex_strings = wm->texture_names.data();
	num_tex_strings = wm->texture_names.size();

	
	if (inside_map) {
		mark_bad_faces();
	}

	if (enable_radiosity) {
		find_coplanar_faces();
		make_patches();
		subdivide_patches();
	}

	add_lights(wm);

	printf("Total patches: %u\n", num_patches);

	u32 start_light_face = SDL_GetTicks();
	printf("Lighting faces...\n");
	const brush_model_t* bm = &wm->models[0];	// "worldspawn"

	run_threads_on_function(&light_face, bm->face_start, bm->face_start + bm->face_count);

	printf("Total pixels: %u\n", total_pixels);
	printf("Total raycasts: %u\n", total_rays_cast);
	printf("Face lighting time: %u\n", SDL_GetTicks() - start_light_face);

	if (enable_radiosity) {
		printf("Making transfers...\n");

		run_threads_on_function(&make_transfers, 0, num_patches);

		printf("Finished transfers\n");

		printf("Bouncing light...\n");
		start_radiosity();

		for (int i = 0; i < amb_cube_transfers.size(); i++) {
			sample_ambient_cubes(i);
		}
	}

	printf("Final light pass...\n");

	for (int i = bm->face_start; i < bm->face_start + bm->face_count; i++) {
		final_light_face(i);
	}


	final_lightmap.resize(lm_width * lm_height * 3, 0);
	for (int i = 0; i < final_lightmap.size(); i += 3) {
		final_lightmap[i] = 255;
		final_lightmap[i+1] = 0;
		final_lightmap[i+2] = 255;

	}

	printf("Writing output...\n");

	std::sort(images.begin(), images.end(), [](const Image& i1, const Image& i2)
		{ return i1.height * i1.width > i2.height * i2.width; });


	root = new PackNode;
	root->rc.x = 0;
	root->rc.y = 0;
	root->rc.height = lm_height;
	root->rc.width = lm_width;
	for (int i = 0; i < images.size(); i++) {
		Image* img = &images.at(i);
		img->height;// += 2;
		img->width;// += 2;

		auto pnode = insert_imgage(root, &images.at(i));
		if (!pnode) {
			printf("Not enough room in lightmap\n");
			exit(1);
		}
		pnode->image_id = i;
		// insert image here

		append_to_lightmap(pnode, &images.at(i));

		face_t* f = &faces[images.at(i).face_num];
		f->lightmap_min[0] = pnode->rc.x;// +1;
		f->lightmap_min[1] = pnode->rc.y;// +1;

		f->lightmap_size[0] = pnode->rc.width;// -1;
		f->lightmap_size[1] = pnode->rc.height;// -1;
	}

	delete root;

	stbi_write_bmp(("resources/textures/" + wm->name + "_lm.bmp").c_str(), lm_width, lm_height, 3, final_lightmap.data());
	printf("Lightmap finished in %u ms\n", SDL_GetTicks() - start);
	print_rays_cast();
	cur_mode = PatchDebugMode::RADCOLOR;
	create_patch_view(PatchDebugMode::RADCOLOR);

	for (int i = 0; i < ambient_cubes.size(); i++) {
		AmbientCube ac;
		ac.position = ambient_cubes.at(i);
		AmbCubeTransfer& act = amb_cube_transfers.at(i);
		if (act.inside_wall)
			continue;
		for (int j = 0; j < 6; j++) {
			ac.axis_colors[j] = act.sides[j].total_light;
		}

		wm->ambient_grid.push_back(ac);
	}


	ambient_voxel_debug();

	free_patches();
}
#include "glad/glad.h"
void draw_lightmap_debug()
{
	va.draw_array();
	glPointSize(4);
	point_array.draw_array();
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	voxels.draw_array();
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	
}
void draw_lightmap_patches()
{
	patch_va.draw_array();
}