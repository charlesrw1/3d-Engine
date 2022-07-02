#include "Light.h"
#include "WorldGeometry.h"
#include "SDL2/SDL_timer.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <random>

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

struct facelight_t
{
	int num_points = 0;
	vec3* sample_points = nullptr;

	int num_pixels = 0;
	vec3* pixel_colors = nullptr;

	int width=0, height = 0;
};

facelight_t facelights[0x10000];

vec3 radiosity[MAX_PATCHES];
vec3 illumination[MAX_PATCHES];


std::vector<u8> data_buffer;

std::vector<u8> final_lightmap;
int lm_width = 1200;
int lm_height = 1200;
// how many texels per 1.0 meters/units
// 32 quake units = 1 my units
// this is how many actual pixels are in the lightmap, each pixel gets supersampled with 4 additional samples 
float density_per_unit = 8.f;
// Determines size of a patch, more patches dramatically slow radiosity lighting time (its O(n^2))
float patch_grid = 2.0f;
// Number of bounce iterations, you already pay the price computing form factors so might as well set it high
int num_bounces = 64;
// False means direct only
bool enable_radiosity = true;
// determine if "outward facing" faces are culled and not computed, helps performance and file space for Quake and indoor maps
bool inside_map = true;
// Cast a ray from the patch to the center of a face to prevent light bleeds, breaks on some maps
bool test_patch_visibility = true;

VertexArray va;
VertexArray point_array;

struct planeneighbor_t
{
	planeneighbor_t* next = nullptr;
	int face = 0;
};
static int num_planes = 0;
planeneighbor_t planes[MAX_PATCHES];
int index_to_plane_list[MAX_PATCHES];

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

PackNode* root = nullptr;


struct Image
{
	int width, height;
	int buffer_start;

	int face_num;
};

std::vector<Image> images;

bool vec3_compare(vec3& v1, vec3& v2, float epsilon)
{
	return fabs(v1.x - v2.x) < epsilon && fabs(v1.y - v2.y) < epsilon && fabs(v1.z - v2.z) < epsilon;
}

const float RADIAL = patch_grid*4.f;
const float RADIALSQRT = sqrt(RADIAL);

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
	if (face->dont_draw) {
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


	texture_info_t* ti = tinfo + face->t_info_idx;
	std::string* t_string = tex_strings + ti->t_index;
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

	if (ti->flags & SURF_EMIT) {
		p->total_light = vec3(1.f);
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
		if (min[i]+patch_grid+0.25f <= max[i]) {
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

VertexArray patch_va;
static PatchDebugMode cur_mode;

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
std::vector<float> transfers;	// work buffer
void make_transfers(int patch_num)
{
	patch_t* patch = &patches[patch_num];
	plane_t plane = faces[patch->face].plane;
	const int i = patch_num;
	float total = 0;
	vec3 center_with_offset = patch->center + plane.normal * 0.01f;
	patch->num_transfers = 0;
	for (int j = 0; j < num_patches; j++)
	{
		transfers[j] = 0;

		if (j == i) {
			continue;
		}
		patch_t* other = &patches[j];
		vec3 dir = normalize(other->center - patch->center);
		float angi = dot(dir, plane.normal);
		if (angi <= 0) {
			continue;
		}

		float dist_sq = dot(other->center - patch->center, other->center - patch->center);

		plane_t* planej = &faces[other->face].plane;
		float angj = -dot(dir, planej->normal);
		float factor = (angj * angi * other->area) / (dist_sq);

		if (factor <= 0.00005) {
			continue;
		}
		auto res = global_world.tree.test_ray_fast(center_with_offset, other->center + planej->normal * 0.01f);
		if (res.hit) {
			continue;
		}

		if (factor > 0.00005) {
			transfers[j] = factor;
			total += factor;
			patch->num_transfers++;
		}
	}
	float final_total = 0;
	if (patch->num_transfers > 0) {
		patch->transfers = new transfer_t[patch->num_transfers];

		int t_index = 0;
		transfer_t* t = &patch->transfers[t_index++];
		for (int i = 0; i < num_patches; i++) {
			if (transfers[i] <= 0.00005) {
				continue;
			}
			t->factor = transfers[i] / total;
			t->patch_num = i;
			final_total += t->factor;

			t = &patch->transfers[t_index++];
			assert(t_index <= patch->num_transfers+1);
		}
	}
}
void shoot_light(int patch_num)
{
	patch_t* p = &patches[patch_num];

	vec3 send = radiosity[patch_num];

	for (int i = 0; i < p->num_transfers; i++) {
		transfer_t* t = p->transfers + i;
		illumination[t->patch_num] += send * t->factor;
	}
}
void collect_light()
{
	for (int i = 0; i < num_patches; i++) {

		patch_t* p = patches + i;
		p->total_light += illumination[i] / p->area;
		radiosity[i] = illumination[i] * p->reflectivity;
		illumination[i] = vec3(0.f);
	}
}
void start_radiosity()
{
	for (int i = 0; i < num_patches; i++)
	{
		patch_t* p = &patches[i];
		radiosity[i] = p->sample_light * p->reflectivity * p->area;

	}
	for (int i = 0; i < num_bounces; i++)
	{
		for (int j = 0; j < num_patches; j++) {
			shoot_light(j);
		}
		collect_light();
	}

}


void free_patches()
{
	for (int i = 0; i < num_patches; i++) {
		delete[] patches[i].transfers;
	}
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
enum light_type_t
{
	LIGHT_POINT,
	LIGHT_SURFACE,
};
struct light_t
{
	vec3 pos;
	vec3 color;
	vec3 normal;

	light_type_t type;

	int brightness;
};
std::vector<light_t> lights;
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

		//	va->push_2({ l.face->texture_axis[i]*min[i],vec3(0,1,1) }, { vec3(0),vec3(0,1,1) });
		//	va->push_2({ l.face->texture_axis[i] * max[i],vec3(0,0.8,1) }, { vec3(0),vec3(0,1,0.8) });

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
const float of_dist = 0.05f;
const float sample_ofs[4][2] = { {of_dist,of_dist},{of_dist,-of_dist},{-of_dist,-of_dist},{-of_dist,of_dist} };

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
				vec3 light_p = lights[i].pos;
				vec3 sample_pos = l.sample_points[s][j];

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

				float surface_scale = 1.f;
				if (lights[i].type == LIGHT_SURFACE) {
					surface_scale = -dot(lights[i].normal, light_dir);
					surface_scale = glm::max(surface_scale, 0.f);
				}


				vec3 final_color = lights[i].color * dif * max(1 / (dist * dist + lights[i].brightness) * (lights[i].brightness - dist), 0.f) * surface_scale;
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
			fl->pixel_colors[i] += radial.get_ambient(i);
		}
	}
	
	for (int y = 0; y < img.height; y++) {
		for (int x = 0; x < img.width; x++) {
			vec3 total = vec3(0);
			int added = 0;
			int offset = y * img.width + x;
			total = fl->pixel_colors[y * img.width + x];
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
		if (find->second == "light_torch_small_walltorch") {
			color = vec3(1.0, 0.3, 0.0);
			brightness = 100;
			//continue;
		}
		else if (find->second == "light_flame_large_yellow") {
			color = vec3(1.0, 0.5, 0.0);
			brightness = 400;
			//continue;
		}
		else if (find->second == "light") {

		}
		else if (find->second == "light_fluoro") {
			color = vec3(0.8, 0.8, 1.0);
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

		light_t l;
		l.pos = org;
		l.color = color;
		l.brightness = brightness;
		l.type = LIGHT_POINT;

		lights.push_back(l);
	}
	for (int i = 0; i < num_patches; i++)
	{
		patch_t* p = patches + i;
		float light_amt = dot(p->total_light, vec3(1.f));
		if (light_amt < 1.f) {
			continue;
		}
		
		texture_info_t* ti = tinfo + faces[p->face].t_info_idx;

		light_t l;
		// brightness value is stored in the x texture scale
		l.brightness = ti->uv_scale[0] * p->area;
		l.color = vec3(1.f);
		l.type = LIGHT_SURFACE;
		l.normal = faces[p->face].plane.normal;
		l.pos = p->center + l.normal * 0.01f;
		
		lights.push_back(l);
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


#include <algorithm>
void create_light_map(worldmodel_t* wm)
{
	world = wm;
	u32 start = SDL_GetTicks();
	printf("Starting lightmap...\n");
	va.init(VertexArray::Primitive::LINES);
	point_array.init(VertexArray::Primitive::POINTS);


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
	// Used for random debug colors
	srand(time(NULL));

	printf("Total patches: %u\n", num_patches);

	u32 start_light_face = SDL_GetTicks();
	printf("Lighting faces...\n");
	const brush_model_t* bm = &wm->models[0];	// "worldspawn"
	for (int i = bm->face_start; i < bm->face_start + bm->face_count; i++) {
		light_face(i);
	}
	printf("Total pixels: %u\n", total_pixels);
	printf("Total raycasts: %u\n", total_rays_cast);
	printf("Face lighting time: %u\n", SDL_GetTicks() - start_light_face);

	if (enable_radiosity) {
		printf("Making transfers...\n");
		transfers.resize(MAX_PATCHES);
		for (int i = 0; i < num_patches; i++) {
			make_transfers(i);
		}
		printf("Finished transfers\n");

		printf("Bouncing light...\n");
		start_radiosity();
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

	stbi_write_bmp("resources/textures/lightmap.bmp", lm_width, lm_height, 3, final_lightmap.data());
	printf("Lightmap finished in %u ms\n", SDL_GetTicks() - start);
	
	cur_mode = PatchDebugMode::RADCOLOR;
	create_patch_view(PatchDebugMode::RADCOLOR);

	free_patches();
}
#include "glad/glad.h"
void draw_lightmap_debug()
{
	va.draw_array();
	glPointSize(4);
	point_array.draw_array();
	
}
void draw_lightmap_patches()
{
	patch_va.draw_array();
}