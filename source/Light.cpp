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

#include <map>

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

int lm_width = 1500;
int lm_height = 1500;
// how many texels per 1.0 meters/units
// 32 quake units = 1 my units
float density_per_unit = 8.f;

float patch_grid = 0.5f;
int num_bounces = 100;

VertexArray va;
VertexArray point_array;

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
struct triangle_t;
struct triedge_t
{
	plane_t plane;	// extends perpendicular to triangle normal
	triangle_t* tri = nullptr;
	int p0=0, p1=0;
};
struct triangle_t
{
	triedge_t* edges[3] = { nullptr,nullptr,nullptr };
};
#define MAX_TRI_POINTS 2048
#define MAX_TRI_EDGES (MAX_TRI_POINTS*6)
#define MAX_TRI_TRIS (MAX_TRI_POINTS*2)
// shamelessly stolen from Quake 2...
struct triangulation_t
{
	triangulation_t() {
		memset(patch_points, 0, sizeof(patch_points));
		memset(edge_matrix, 0, sizeof(edge_matrix));
		memset(used_points, 0, sizeof(used_points));

	}
	int num_points = 0;
	int num_edges = 0;
	int num_tris = 0;

	const face_t* face = nullptr;
	patch_t* patch_points[MAX_TRI_POINTS];
	bool used_points[MAX_TRI_POINTS];
	triedge_t edges[MAX_TRI_EDGES];
	triangle_t tries[MAX_TRI_TRIS];

	triedge_t* edge_matrix[MAX_TRI_POINTS][MAX_TRI_POINTS];

	vec3 color;

	void triedge_r(triedge_t* e) {
		if (e->tri)
			return;
		vec3 p0 = patch_points[e->p0]->center;
		vec3 p1 = patch_points[e->p1]->center;
		float best = 1.1;
		int best_point = -1;
		for (int i = 0; i < num_points; i++) {
			vec3 point = patch_points[i]->center;
			if (e->plane.dist(point) < 0)
				continue;
			vec3 v1 = normalize(p0 - point);
			vec3 v2 = normalize(p1 - point);
			float ang = dot(v1, v2);
			if (ang < best) {
				best = ang;
				best_point = i;
			}
		}
		if (best > 0.9) {
			return;
		}
		if (best_point == -1) {
			return;
		}
		triangle_t* tri = add_triangle();
		tri->edges[0] = e;
		tri->edges[1] = find_edge(e->p1, best_point);
		tri->edges[2] = find_edge(best_point, e->p0);
		for (int i = 0; i < 3; i++) {
			tri->edges[i]->tri = tri;
		}
		triedge_r(find_edge(best_point, e->p1));
		triedge_r(find_edge(e->p0, best_point));
	}
	void lerp_tri(triangle_t* t, vec3 point, vec3& color)
	{
		patch_t* p[3];
		for (int i = 0; i < 3; i++) {
			p[i] = patch_points[t->edges[i]->p0];
		}
		vec3 base = p[0]->total_light;
		vec3 d1 = p[1]->total_light - base;
		vec3 d2 = p[2]->total_light - base;

		float x = dot(point, t->edges[0]->plane.normal) + t->edges[0]->plane.d;
		float y = dot(point, t->edges[2]->plane.normal) + t->edges[2]->plane.d;

		float y1 = dot(p[1]->center, t->edges[2]->plane.normal) + t->edges[2]->plane.d;
		float x2 = dot(p[2]->center, t->edges[0]->plane.normal) + t->edges[0]->plane.d;
		if (fabs(y1) < 0.01f || fabs(x2) < 0.01f) {
			color = base;
		}
		color = base + (x / x2) * d2 + (y / y1) * d1;
	}
	bool point_in_tri(vec3 point, triangle_t* t)
	{
		for (int i = 0; i < 3; i++) {
			triedge_t* e = t->edges[i];
			if (dot(e->plane.normal, point) + e->plane.d < 0) {
				return false;
			}
		}
		return true;
	}
	void add_point(patch_t* p) {
		if (num_points >= MAX_TRI_POINTS) {
			printf("Too many points!\n");
			exit(1);
		}
		this->patch_points[num_points++] = p;
	}
	triangle_t* add_triangle() {
		if (num_tris >= MAX_TRI_TRIS) {
			printf("Too many tris\n");
			exit(1);
		}
		return &this->tries[num_tris++];
	}
	triedge_t* find_edge(int p0, int p1) {
		if (edge_matrix[p0][p1]) {
			return edge_matrix[p0][p1];
		}
		if (num_edges+2 > MAX_TRI_EDGES) {
			printf("Too many tri edges\n");
			exit(1);
		}
		vec3 edge = normalize(patch_points[p1]->center - patch_points[p0]->center);
		vec3 normal = cross(edge, face->plane.normal);
		float dist = -dot(normal, patch_points[p0]->center);

		triedge_t* e = &edges[num_edges++];
		e->p0 = p0;
		e->p1 = p1;
		e->plane.normal = normal;
		e->plane.d = dist;
		edge_matrix[p0][p1] = e;


		triedge_t* be = &edges[num_edges++];
		be->p0 = p1;
		be->p1 = p0;
		be->plane.normal = -normal;
		be->plane.d = -dist;
		edge_matrix[p1][p0] = be;

		va.push_line(patch_points[p0]->center, patch_points[p1]->center, color);
		used_points[p0] = true;
		used_points[p1] = true;

		return e;
	}
	void triangulate_points()
	{
		if (num_points <= 1)
			return;
		float best_dist = 1000;
		int b1, b2;
		for (int i = 0; i < num_points; i++) {
			vec3 point1 = patch_points[i]->center;
			for (int j = i + 1; j < num_points; j++) {
				vec3 point2 = patch_points[j]->center;
				float dist = length(point2 - point1);
				if (dist < best_dist) {
					b1 = i;
					b2 = j;
					best_dist = dist;
				}
			}
		}
		triedge_t* e = find_edge(b1, b2);
		triedge_t* e2 = find_edge(b2, b1);
		triedge_r(e);
		triedge_r(e2);
	}
	void sample(vec3 point, vec3& color)
	{
		if (num_points == 0) {
			color = vec3(0);
			return;
		}
		if (num_points == 1) {
			color = patch_points[0]->total_light;
			return;
		}
		for (int i = 0; i < num_tris; i++) {
			triangle_t* tri = &tries[i];
			if (!point_in_tri(point, tri))
				continue;
			lerp_tri(tri, point, color);
			return;
		}
		for (int i = 0; i < num_edges; i++) {
			triedge_t* te = &edges[i];
			if (te->tri)
				continue;
			if (te->plane.dist(point) < 0)
				continue;
			vec3 p0 = patch_points[te->p0]->center;
			vec3 p1 = patch_points[te->p1]->center;

			vec3 v1 = normalize(p1 - p0);
			vec3 v2 = point - p0;
			float d = dot(v2, v1);
			if (d < 0)
				continue;
			if (d > 1)
				continue;
			patch_t* pp0 = patch_points[te->p0];
			patch_t* pp1 = patch_points[te->p1];
			color = pp0->total_light + d * (pp1->total_light - pp0->total_light);
			return;

		}
		float best_d = 10000;
		patch_t* best_p = nullptr;
		for (int i = 0; i < num_points; i++) {
			vec3 p0 = patch_points[i]->center;
			float dist = length(point - p0);
			if (dist < best_d)
			{
				best_d = dist;
				best_p = patch_points[i];
			}
		}
		if (!best_p) {
			printf("Triangulation with no points\n");
			exit(1);
		}
		color = best_p->total_light;
	}

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
	assert(num_patches < 0x1000);
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
		p->reflectance = rgb_to_float(237, 28, 36);
	}
	else if (*t_string == "color/green") {
		p->reflectance = rgb_to_float(34,177,76);
	}
	else if (*t_string == "color/blue") {
		p->reflectance = rgb_to_float(0, 128, 255);
	}
	else if (*t_string == "color/yellow") {
		p->reflectance = rgb_to_float(255, 242, 0);
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
/*

	for (i = 0; i < 3; i++) {
		//(floor(((min[i]+0.05)* some_num))/ some_num / patch_grid)
		//(ceil(((max[i] - 0.05)* some_num)/ some_num) / patch_grid)
		float f1 = floor(min[i] + 1);
		float c1 = ceil(max[i] - 1);

		if (f1<=c1) {
			plane_t split;
			split.normal = vec3(0);
			split.normal[i] = 1.f;
			//-(((floor((min[i] + 1) * some_num) / some_num) / patch_grid) + 1) * patch_grid;
			split.d = -(floor(min[i] + 0.1) + 1);
			bool res = try_split_winding(p->winding, split, front, back);
			if (res) {
				break;
			}
			failed_subdivide++;
		}
	}*/
/*

if ((floor(((min[i]+1)* some_num))/ some_num / patch_grid) < (ceil(((max[i] - 1)* some_num)/ some_num) / patch_grid)+0.01f) {
			plane_t split;
			split.normal = vec3(0);
			split.normal[i] = 1.f;
			split.d = -(((floor((min[i] + 1) * some_num) / some_num) / patch_grid) + 1) * patch_grid;
			bool res = try_split_winding(p->winding, split, front, back);
			*/

static int failed_subdivide = 0;
const float some_num = 20.f;
void subdivide_patch(patch_t* p)
{
	vec3 min, max;
	winding_t front, back;
	get_extents(p->winding, min, max);
	int i;
	for (i = 0; i < 3; i++) {
		// Quake 2 had the patch grid as an actual grid thats not relative to a specific patch
		// However (at least in my radiosity), the edges get bright (assuming cause the bounces are more intense there)
		// and is very noticable when the patches are small
		// solution is to have the patch grid start at the face, and also not let a face become 0.25f in width which solves it
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

	new_p->reflectance = p->reflectance;

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
		radiosity[i] = illumination[i] * p->reflectance;
		illumination[i] = vec3(0.f);
	}
}
void start_radiosity()
{
	for (int i = 0; i < num_patches; i++)
	{
		patch_t* p = &patches[i];
		radiosity[i] = p->sample_light * p->reflectance*p->area;
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


const int MAP_PTS = 256 * 256;
//std::vector<vec3> points(MAP_PTS);
//std::vector<vec3> temp_image_buffer(MAP_PTS);
struct LightmapState
{
	//std::vector<vec3> points;
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

	facelight_t* fl;

	int surf_num = 0;
	face_t* face = nullptr;
};
struct light_t
{
	vec3 pos;
	vec3 color;

	int brightness;
};
std::vector<light_t> lights; //= { { {2.f,20.f,-9.f},{0.3f,1.0f,0.4f} } };// , { {-5.f,5.f,8.f},{0.1f,0.3f,1.0f} }, { {0,1,0},{1,1,1} }, { {0.f,10.f,0.f},{2.f,2.0f,2.0f} }, { {2.f,20.f,-9.f},{0.3f,1.0f,0.4f} }, { {0.f,5.f,0.f},{1.f,0.0f,0.0f} } };
static u32 total_pixels = 0;
static u32 total_rays_cast = 0;

struct planeneighbor_t
{
	planeneighbor_t* next=nullptr;
	int face=0;
};
static int num_planes = 0;
planeneighbor_t planes[MAX_PATCHES];
int index_to_plane_list[MAX_PATCHES];
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

void calc_points(LightmapState& l, Image& img)
{
	int h = l.tex_size[1] * density_per_unit + 1;
	int w = l.tex_size[0] * density_per_unit + 1;
	float start_u = l.exact_min[0];	// added -0.25
	float start_v = l.exact_min[1];
	l.numpts = h * w;
	if (l.numpts > MAP_PTS) {
		l.numpts = MAP_PTS;
	}
	total_pixels += l.numpts;

	l.fl->height = h;
	l.fl->width = w;

	l.fl->pixel_colors = new vec3[l.numpts];
	l.fl->sample_points = new vec3[l.numpts];

	l.fl->num_pixels = l.numpts;
	l.fl->num_points = l.numpts;

	img.height = h;
	img.width = w;

	float step[2];
	step[0] = (l.exact_max[0] - l.exact_min[0]) / w;	// added + 0.5
	step[1] = (l.exact_max[1] - l.exact_min[1]) / h;


	int top_point = 0;

	float mid_u = (l.exact_min[0] + l.exact_max[0]) / 2.f;
	float mid_v = (l.exact_min[1] + l.exact_max[1]) / 2.f;
	vec3 face_mid = l.tex_origin + l.tex_to_world[0] * mid_u + l.tex_to_world[1] * mid_v + l.face->plane.normal * 0.01f;
	face_mid = l.face_middle + l.face->plane.normal * 0.01f;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			float u = start_u + (x) * step[0]+step[0]/2.f;
			float v = start_v + (y) * step[1] + step[1] / 2.f;

			vec3 point = l.tex_origin + l.tex_to_world[0] * u + l.tex_to_world[1] * v + l.face->plane.normal * 0.01f;

			/*
			auto res = global_world.tree.test_ray_fast(point + l.face->plane.normal * 0.1f , face_mid + l.face->plane.normal * 0.1f);
			if (res.hit) {
				point = res.end_pos;
				//va->append({ point,vec3(0,1,0) });
			}
			*/

			//point = point + move_mid * 0.25f;
			if (top_point >= MAP_PTS) {
				l.numpts = MAP_PTS - 1;
				img.height = y - 1;
				l.fl->height = y - 1;
				goto face_too_big;
			}
			assert(top_point < MAP_PTS);
			l.fl->sample_points[top_point++] = point;
		}
	}
face_too_big:;
}

void calc_vectors(LightmapState& l)
{
	assert(l.face->t_info_idx >= 0 && l.face->t_info_idx < num_tinfo);
	texture_info_t* ti = &tinfo[l.face->t_info_idx];


	// face vectors
	l.world_to_tex[0] = ti->axis[0];
	l.world_to_tex[1] = ti->axis[1];



	//va->push_2({ l.face->texture_axis[0],vec3(1,0,0) }, { vec3(0),vec3(1,0,0) });
	//va->push_2({ l.face->texture_axis[1],vec3(0,1,0) }, { vec3(0),vec3(0,1,0) });


	l.tex_normal = normalize(cross(ti->axis[1], ti->axis[0]));

	//va->push_2({ l.tex_normal,vec3(0,0,1) }, { vec3(0),vec3(0,0,1) });


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

	//va->push_2({ l.tex_origin,vec3(0.2,0.5,1) }, { vec3(0),vec3(0.2,0.5,1) });s
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

	calc_vectors(l);
	calc_extents(l);
	/*
	vec3 face_mid = l.face_middle + l.face->plane.normal * 0.02f;
	trace_t test_if_outside_map = global_world.tree.test_ray_fast(face_mid, face_mid + l.face->plane.normal * 100.f);
	total_rays_cast++;

	if (!test_if_outside_map.hit) {
		l.face->dont_draw = true;
		return;
	}
	*/
	calc_points(l, img);

	//img.buffer_start = data_buffer.size();
	//make_space(img.height * img.width);
	// ambient term

	for (int i = 0; i < l.numpts; i++) {
		l.fl->pixel_colors[i] = vec3(0);
		//add_color(img.buffer_start, i, vec3(0.05, 0.05, 0.05));
	}

	for (int j = 0; j < l.numpts; j++) {
		for (int i = 0; i < lights.size(); i++) {
			vec3 light_p = lights[i].pos;

			float plane_dist = l.face->plane.distance(light_p);
			if (plane_dist < 0) {
				continue;
			}


			float sqrd_dist = dot(light_p - l.fl->sample_points[j], light_p - l.fl->sample_points[j]);
			if (sqrd_dist > lights[i].brightness * lights[i].brightness) {
				continue;
			}

			total_rays_cast++;
			trace_t res = global_world.tree.test_ray_fast(l.fl->sample_points[j], light_p);
			if (res.hit) {
				continue;
			}
			//va->append({ l.points[j],vec3(0,1,0) });

			vec3 light_dir = light_p - l.fl->sample_points[j];
			float dist = length(light_dir);
			light_dir = normalize(light_dir);
			float dif = dot(light_dir, l.face->plane.normal);
			if (dif < 0) {
				continue;
			}

			vec3 final_color = lights[i].color * dif * max(1 / (dist * dist + lights[i].brightness) * (lights[i].brightness - dist), 0.f);
			l.fl->pixel_colors[j] += final_color;

			//temp_image_buffer[j] = min(temp_image_buffer[j], vec3(1));
			//add_color(img.buffer_start, j, final_color);
		}
		add_sample_to_patch(l.fl->sample_points[j], l.fl->pixel_colors[j], num);
	}

	patch_t* patch = face_patches[num];
	while (patch)
	{
		patch->sample_light /= patch->num_samples;
		patch = patch->next;
	}

	/*
	for (int y = 0; y < img.height; y++) {
		for (int x = 0; x < img.width; x++) {
			vec3 total = vec3(0);
			int added = 0;
			for (int i = -1; i <= 1; i++) {
				for (int j = -1; j <= 1; j++) {
					int ycoord = y + i;
					int xcoord = x + j;
					if (ycoord<0 || ycoord>img.height - 1 || xcoord <0 || xcoord> img.width - 1)
						continue;

					total += temp_image_buffer.at(ycoord * img.width + xcoord);
					added++;

				}

			}
			total /= added;
			int offset = y * img.width + x;
			assert(offset < l.numpts);
			add_color(img.buffer_start, offset, total);
		}
	}
	*/
	
	

	//images.push_back(img);
}

void final_light_face(int face_num)
{
	face_t* face = &faces[face_num];
	facelight_t* fl = &facelights[face_num];
	Image img;
	img.face_num = face_num;
	img.height = fl->height;
	img.width = fl->width;
	img.buffer_start = data_buffer.size();
	make_space(img.height * img.width);

	triangulation_t* t = new triangulation_t;
	t->color = random_color();
	t->face =face;
	patch_t* p = face_patches[face_num];
	while (p)
	{
		t->add_point(p);
		p = p->next;
	}

	t->triangulate_points();

	for (int i = 0; i < fl->num_points; i++) {

		vec3 sample_point = fl->sample_points[i];
		vec3 gi_color;
		t->sample(sample_point, gi_color);

		fl->pixel_colors[i] += gi_color;
	}

	for (int y = 0; y < img.height; y++) {
		for (int x = 0; x < img.width; x++) {
			vec3 total = vec3(0);
			int added = 0;
			for (int i = -1; i <= 1; i++) {
				for (int j = -1; j <= 1; j++) {
					int ycoord = y + i;
					int xcoord = x + j;
					if (ycoord<0 || ycoord>img.height - 1 || xcoord <0 || xcoord> img.width - 1)
						continue;

					total += fl->pixel_colors[ycoord * img.width + xcoord];
					added++;
				}
			}
			total /= added;
			int offset = y * img.width + x;
			//assert(offset < l.numpts);
			add_color(img.buffer_start, offset, total);
		}
	}



	images.push_back(img);



	delete[] fl->pixel_colors;
	delete[] fl->sample_points;
	delete t;
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
		for (int x = 0; x < img->width; x++) {

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
		int brightness = 200;
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

		lights.push_back({ org,color,brightness });

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
void temp_check_triangulation()
{
	for (int i = 0; i < num_faces; i++)
	{
		triangulation_t* t = new triangulation_t;
		t->color = random_color();
		t->face = &faces[i];
		patch_t* p = face_patches[i];
		while (p)
		{
			t->add_point(p);
			p = p->next;
		}

		t->triangulate_points();

		for (int j = 0; j < t->num_points; j++) {
			if (!t->used_points[j]) {
				point_array.append(VertexP(t->patch_points[j]->center, t->color));
			}
		}

		delete t;
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

	add_lights(wm);

	mark_bad_faces();

	//find_coplanar_faces();

	make_patches();
	subdivide_patches();
	srand(234508956l);
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

	printf("Making transfers...\n");
	transfers.resize(MAX_PATCHES);
	for (int i = 0; i < num_patches; i++) {
		make_transfers(i);
	}
	printf("Finished transfers\n");

	printf("Bouncing light...\n");
	start_radiosity();

	printf("Final light pass...\n");
	for (int i = bm->face_start; i < bm->face_start + bm->face_count; i++) {
		final_light_face(i);
	}


	final_lightmap.resize(lm_width * lm_height * 3, 0);
	for (int i = 0; i < final_lightmap.size(); i += 3) {
		//final_lightmap[i] = 255;
		//final_lightmap[i+1] = 0;
		//final_lightmap[i+2] = 255;

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
		auto pnode = insert_imgage(root, &images.at(i));
		if (!pnode) {
			printf("Not enough room in lightmap\n");
			exit(1);
		}
		pnode->image_id = i;
		// insert image here

		append_to_lightmap(pnode, &images.at(i));

		face_t* f = &faces[images.at(i).face_num];
		f->lightmap_min[0] = pnode->rc.x;
		f->lightmap_min[1] = pnode->rc.y;

		f->lightmap_size[0] = pnode->rc.width;
		f->lightmap_size[1] = pnode->rc.height;
	}

	delete root;

	stbi_write_bmp("resources/textures/lightmap.bmp", lm_width, lm_height, 3, final_lightmap.data());
	printf("Lightmap finished in %u ms\n", SDL_GetTicks() - start);
	
	cur_mode = PatchDebugMode::RADCOLOR;
	create_patch_view(PatchDebugMode::RADCOLOR);

	//temp_check_triangulation();

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