#include "Light.h"
#include "WorldGeometry.h"
#include "SDL2/SDL_timer.h"
#include "ThreadTools.h"
#include <algorithm>
#include "Voxelizer.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "Sampling.h"
#include "BVHTREE.h"

worldmodel_t* world;
const worldmodel_t* GetWorld() { return world; }

LightmapSettings config;
const LightmapSettings* GetConfig() { return &config; }

BrushTree brush_tree;
const BrushTree* GetBrushTree() { return &brush_tree; }


// ---------- DEBUG ---------------
VertexArray va;
VertexArray point_array;
VertexArray voxels;
VertexArray patch_va;
// ---------------------------------



// ------------ UTILITIES --------------

vec3 lerp(const vec3& a, const vec3& b, float percentage)
{
	return a + (b - a) * percentage;
}

vec3 RGBToFloat(unsigned char r, unsigned char g, unsigned char b) {
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


struct Sample
{
	vec3 color;
	vec3 position;
	vec3 normal;
	bool edge;
	bool occluded;
};

struct Image
{
	int width, height;
	int buffer_start;

	int face_num;
};

class SampleBuffer
{
public:
	void AddImage(Image image) {
		images.push_back(image);
	}
	int MakeSpace(int num_samples) {
		int start = samples.size();
		samples.resize(samples.size() + num_samples);
		return start;
	}
	void AddSample(const Image& img, int sample_num, const Sample& sample) {
		samples[img.buffer_start + sample_num] = sample;
	}


	std::vector<Image> images;
	std::vector<Sample> samples;
};

static SampleBuffer sample_buffer;


// ---------- LIGHTMAP IMAGE --------
//PackNode* root = nullptr;
//std::vector<Image> images;
//std::vector<u8> data_buffer;
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
	const face_t* f = &world->faces[face_idx];
	vec3 V0 = world->verts[f->v_start] - world->verts[f->v_start + 1];
	vec3 V1 = world->verts[f->v_start + 1] - world->verts[f->v_start + 2];

	float u = random_float();
	float v = random_float();

	return world->verts[f->v_start + 2] + u * V0 + v * V1;
}


 float RADIAL = config.patch_grid*4.f;
 float RADIALSQRT = sqrt(RADIAL);

void create_patch_view(PatchDebugMode mode)
{
	using VP = VertexP;
	using PDM = PatchDebugMode;
	patch_va.init(VAPrim::TRIANGLES);
	for (int i = 0; i < patches.size(); i++) {
		patch_t* p = &patches[i];
		winding_t* w = &p->winding;
		vec3 color;
		switch (mode)
		{
		case PDM::RANDOM:
			color = random_color();
			break;
		case PDM::AREA:
			color = vec3(p->area / (config.patch_grid*config.patch_grid*5.f));
			break;
		case PDM::LIGHTCOLOR:
			color = p->sample_light;
			break;
		case PDM::NUMTRANSFERS:
			color = vec3(p->num_transfers * 2.f / (patches.size()));
			break;
		case PDM::RADCOLOR:
			color =  pow(p->total_light,vec3(1/2.2f));
			break;
		}
		for (int i = 0; i < w->num_verts-2; i++) {
		//	patch_va.push_line(w->v[i], w->v[(i + 1)%w->num_verts], vec3(1.0));
			patch_va.push_3(VP(w->v[0], color), VP(w->v[i + 1], color), VP(w->v[(i + 2) % w->num_verts],color));
		}
	}
}

const int NUM_SKY_SAMPLES = 36;
const float SKY_DISTANCE = 300.f;

float GatherSkyVisibility(vec3 p, vec3 N)
{
	int sky_hit = 0;
	for (int i = 0; i < NUM_SKY_SAMPLES; i++)
	{
		vec3 direction = sample_hemisphere_cos(N, i, NUM_SKY_SAMPLES-1);
		trace_t res = global_world.tree.test_ray_fast(p, p + direction * SKY_DISTANCE);
		if (!res.hit|| world->t_info[world->faces[res.face].t_info_idx].flags & SURF_SKYBOX)
			sky_hit++;

	}
	return sky_hit / (float)NUM_SKY_SAMPLES;
}

const float SKYVIS_THRESHOLD = 0.1;

std::vector<float> transfers;
void MakeTransfers(int patch_num)
{
	patch_t* patch = &patches[patch_num];
	if (patch->occluded)
		return;

	plane_t plane = world->faces[patch->face].plane;
	const int i = patch_num;
	float total = 0;
	vec3 center_with_offset = patch->center + plane.normal * 0.01f;
	patch->num_transfers = 0;
	for (int j = 0; j < patches.size(); j++)
	{
		transfers[j] = 0;
		if (j == i)
			continue;
		patch_t* other = &patches[j];

		if (other->occluded)
			continue;

		vec3 dir = normalize(other->center - patch->center);
		float angi = dot(dir, plane.normal);
		if (angi <= 0)
			continue;

		float dist_sq = dot(other->center - patch->center, other->center - patch->center);

		const plane_t* planej = &world->faces[other->face].plane;
		float angj = -dot(dir, planej->normal);
		float factor = (angj * angi * other->area) / (dist_sq);
		if (factor <= 0.00005)
			continue;
		
		auto res = global_world.tree.test_ray_fast(center_with_offset, other->center + planej->normal * 0.01f);
		if (res.hit)
			continue;

		if (factor > 0.00005) {
			transfers[j] = factor;
			total += factor;
			patch->num_transfers++;
		}
	}
	
	// radiosity equation should sum to PI, leftover is the factor that isnt seen by a surface, assume its the sky
	float temp_sky_vis = max(3.14159f - total,0.f)/3.14159f;
	if (temp_sky_vis > SKYVIS_THRESHOLD)
	{
		float real_sky_vis = GatherSkyVisibility(center_with_offset, plane.normal);
		
		float delta = abs(real_sky_vis - temp_sky_vis);
		if (real_sky_vis>SKYVIS_THRESHOLD) {
			patch->sky_visibility = temp_sky_vis;
		}
		
		
		//patch->sky_visibility = real_sky_vis;
	}
	total = max(total, 3.14159f);
	//total = max(total, 3.14159f);
	//patch->sky_visibility /= total;


	if (patch->num_transfers > 0) {
		patch->transfers = new transfer_t[patch->num_transfers];

		int t_index = 0;
		transfer_t* t = &patch->transfers[t_index++];
		for (int i = 0; i < patches.size(); i++) {
			if (transfers[i] <= 0.00005) {
				continue;
			}
			t->factor = transfers[i] / total;
			t->patch_num = i;
			t = &patch->transfers[t_index++];
			assert(t_index <= patch->num_transfers + 1);
		}
			
	}
}

void FreePatches()
{
	for (int i = 0; i < patches.size(); i++) {
		delete[] patches[i].transfers;
	}
}

void RadWorld()
{

	std::vector<vec3> radiosity;

	radiosity.resize(patches.size());

	for (int i = 0; i < patches.size(); i++) {
		patch_t* p = &patches[i];
		radiosity[i] = p->sample_light * p->reflectivity + p->emission;
	}
	for (int i = 0; i < config.num_bounces; i++)
	{
		// Gather light
		for (int j = 0; j < patches.size(); j++)
		{
			vec3 total = vec3(0.0);
			patch_t* p = &patches[j];
			total += p->sky_visibility * GetSky().sky_color;

			for (int t = 0; t < p->num_transfers; t++)
			{
				transfer_t* tran = p->transfers + t;
				total += radiosity[tran->patch_num] * tran->factor;
			}
			// Incoming irradiance
			p->total_light = total;
		}
		// Collect light
		for (int j = 0; j < patches.size(); j++)
		{
			patch_t* p = &patches[j];

			radiosity[j] = (p->total_light + p->sample_light) * p->reflectivity + p->emission;
		}

	}
}


struct PatchPointType
{
	int patch_index{};
	vec3 GetPoint() const {
		return patches[patch_index].center;
	}
};

static PointTree<PatchPointType> patch_tree;

void CreatePatchTree()
{
	for (int i = 0; i < patches.size(); i++) {
		if (patches[i].occluded)
			continue;
		PatchPointType ppt;
		ppt.patch_index = i;

		patch_tree.points.push_back(ppt);
	}

	patch_tree.Build();
}





#define IS_OCCLUDED 1
#define OCCLUDER_EDGE 2
const int MAX_POINTS = 256 * 256;
struct LightmapState
{
	vec3 face_middle = vec3(0);
	vec3 plane_dir;
	float plane_d;

	vec3 tex_normal;
	vec3 tex_origin = vec3(0);

	vec3 world_to_tex[2];	// u = dot(world_to_tex[0], world_pos-origin)
	vec3 tex_to_world[2];	// world_pos = tex_origin + u * tex_to_world[0]

	int numpts = 0;
	int height = 0;
	int width = 0;


	int tex_min[2];
	int tex_size[2];
	float exact_min[2];
	float exact_max[2];

	//facelight_t* fl=nullptr;

	// For supersampling, final facelight_t points are the center of the 4 (they might get shifted around)
	std::vector<vec3> sample_points;
	std::vector<float> occlusion;
	std::vector<unsigned char> occluded;
	std::vector<vec3> pixel_colors;

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


void calc_extents(LightmapState& l)
{
	const texture_info_t* ti = &world->t_info[l.face->t_info_idx];
	// Calc face min and max
	float min[2], max[2];
	min[0] = min[1] = 50000;
	max[0] = max[1] = -50000;
	int vcount = l.face->v_count;
	l.face_middle = vec3(0);
	for (int i = 0; i < vcount; i++) {

		l.face_middle += world->verts[l.face->v_start + i];

		for (int j = 0; j < 2; j++) {

			float val = dot(world->verts[l.face->v_start + i], ti->axis[j]);
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
	int h = l.tex_size[1] * config.pixel_density + 1;
	int w = l.tex_size[0] * config.pixel_density + 1;
	float start_u = l.exact_min[0];	
	float start_v = l.exact_min[1];
	l.numpts = h * w;
	if (l.numpts > MAX_POINTS) {
		l.numpts = MAX_POINTS;
	}
	total_pixels += l.numpts;

//	l.fl->height = h;
//	l.fl->width = w;
//
//	l.fl->num_pixels = l.numpts;
//	l.fl->num_points = l.numpts;

	l.sample_points.resize(l.numpts);
	l.pixel_colors.resize(l.numpts,vec3(0));
	l.occlusion.resize(l.numpts,0);
	l.occluded.resize(l.numpts,0);
	l.height = h;
	l.width = w;

	img.height = h;
	img.width = w;

	float step[2];
	step[0] = (l.exact_max[0] - l.exact_min[0]) / (w-1);
	step[1] = (l.exact_max[1] - l.exact_min[1]) / (h-1);

	const face_t* f = l.face;
	winding_t wind;
	for (int i = 0; i < f->v_count; i++) {
		wind.add_vert(world->verts[f->v_start + i]);
	}

	//l.fl->pixel_colors = new vec3[l.numpts];
	//l.fl->sample_points = new vec3[l.numpts];

	vec3 face_mid = l.face_middle + l.face->plane.normal * 0.01f;

	//for (int s = 0; s < l.num_samples; s++) {
		int top_point = 0;
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				float ofs[2];
				ofs[0] = 0;
				ofs[1] = 0;

				float u = start_u + (x)*step[0] + ofs[0];
				float v = start_v + (y)*step[1] + ofs[1];

				vec3 point = l.tex_origin + l.tex_to_world[0] * u + l.tex_to_world[1] * v + l.face->plane.normal * 0.01f;


				// is the point outside the bounds?
				//		cast ray towards middle
				//		did it hit?
				//			no, point is good
				//			yes, find closest point on face winding, move sample to that position

				//	auto trace_res = global_world.tree.test_ray_fast(point, face_mid);
				bool inside =  brush_tree.PointInside(point);
				if (!wind.point_inside(point)||inside) {
					//if (trace_res.hit) {
					//	point = wind.closest_point_on_winding(point);
					//}
					l.occluded[top_point++] = 1;
					continue;
				}

				if (top_point >= MAX_POINTS) {
					l.numpts = MAX_POINTS - 1;
					l.height = y - 1;
					img.height = y - 1;
					//l.fl->height = y - 1;
					goto face_too_big;
				}
				assert(top_point < MAX_POINTS);
				l.sample_points[top_point++] = point;
			}
		}
	face_too_big:;
	//}
}

void calc_vectors(LightmapState& l)
{
	assert(l.face->t_info_idx >= 0 && l.face->t_info_idx < num_tinfo);
	const texture_info_t* ti = &world->t_info[l.face->t_info_idx];


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

void FloodFillOccludedSamples(LightmapState* l)
{
	for (int y = 0; y < l->height; y++) {
		for (int x = 0; x < l->width; x++) {
			const int i = y * l->width + x;
			if (!(l->occluded[i]&IS_OCCLUDED))
				continue;
			vec3 sum = vec3(0);
			int sum_cnt = 0;
			for (int y0 = -1; y0 <= 1; y0++) {
				for (int x0 = -1; x0 <= 1; x0++) {
					int y1 = y + y0;
					int x1 = x + x0;
					if (y1 < 0 || y1 >= l->height || x1 < 0 || x1 >= l->width)
						continue;
					const int j = y1 * l->width + x1;
					if (!(l->occluded[j]&IS_OCCLUDED)) {
						sum += l->pixel_colors[j];
						sum_cnt++;

						l->occluded[j] = OCCLUDER_EDGE;
					}

				}
			}
			//if (sum_cnt > 0)
			//	l->pixel_colors[i] = sum / (float)sum_cnt;
			//else
			//	l->pixel_colors[i] = vec3(0, 1, 1);
		}
	}
}
void BoxBlur(LightmapState* l,int radius)
{
	std::vector<vec3> final_colors;
	final_colors.resize(l->numpts);
	for (int y = 0; y < l->height; y++) {
		for (int x = 0; x < l->width; x++) {
			const int i = y * l->width + x;
			// Edges must maintain their sample colors or the seams become obvious
			if (x == 0 || x == l->width - 1 || y == 0 || y == l->height - 1 || l->occluded[i]&OCCLUDER_EDGE) {
				final_colors[i] = l->pixel_colors[i];
				continue;
			}
			vec3 sum_color = vec3(0);
			float sum_weight = 0;
			for (int y0 = -radius; y0 <= radius; y0++) {
				for (int x0 = -radius; x0 <= radius; x0++) {
					int y1 = clamp(y + y0,0,l->height-1);
					int x1 = clamp(x + x0,0,l->width-1);
					const int j = y1 * l->width + x1;
					if (!(l->occluded[j]&IS_OCCLUDED)) {
						sum_color += l->pixel_colors[j];
						sum_weight += 1.f;
					}
				}
			}
			if (sum_weight > 0)
				final_colors[i] = sum_color / sum_weight;
			else
				final_colors[i] = vec3(0, 1, 1);
		}
	}
	l->pixel_colors = std::move(final_colors);
}
float GetDirtValue(float occlusion)
{
	float out = pow(occlusion, config.dirt_gain);
	out *= config.dirt_scale;
	out = clamp(out,0.f, 1.f);
	return out;
}
void CalculateDirtMap(LightmapState* l)
{
	for (int j = 0; j < l->numpts; j++) {
		if (l->occluded[j])
			continue;

		vec3 sample_pos = l->sample_points[j];
		float occlusion = 0;
		for (int i = 0; i < config.num_dirt_vectors; i++) {
			vec3 dirtvec = sample_hemisphere_cos(l->face->plane.normal, i, config.num_dirt_vectors);
			auto res = global_world.tree.test_ray_fast(sample_pos, sample_pos + dirtvec * config.dirt_dist);
			if (res.hit) {
				occlusion += min(config.dirt_dist, res.length);
			}
			else {
				occlusion += config.dirt_dist;
			}
		}
		l->occlusion[j] = occlusion / (config.num_dirt_vectors * config.dirt_dist);
	}
}
vec3 CalcDirectLightingAtPoint(vec3 point, vec3 normal)
{
	const vec3 sample_pos = point;
	vec3 sum = vec3(0);
	for (const auto& light : LightList()) {
		vec3 v = light.pos - sample_pos;
		if (dot(v, normal) < 0)
			continue;
		float sqred_dist = dot(v, v);
		float scale = light.brightness / sqred_dist;
		if (scale < 0.05)
			continue;

		trace_t res = global_world.tree.test_ray_fast(sample_pos, light.pos);
		if (res.hit)
			continue;

		float dist = sqrt(sqred_dist);
		vec3 dir = v / dist;


		sum += light.color * scale * dot(dir,normal);
	}
	return sum;
}

void CalcFaceDirectLighting(LightmapState* l, const light_t* light)
{
	float plane_dist = l->face->plane.dist(light->pos);
	if (plane_dist < 0)
		return;
	for (int s = 0; s < l->sample_points.size(); s++)
	{
		if (l->occluded[s])
			continue;
		const vec3 sample_pos = l->sample_points[s];
		vec3 v = light->pos - sample_pos;
		float sqred_dist = dot(v, v);
		float scale = light->brightness / sqred_dist;
		if (scale < 0.05)
			continue;

		trace_t res = global_world.tree.test_ray_fast(sample_pos, light->pos);
		if (res.hit)
			continue;

		float dist = sqrt(sqred_dist);
		vec3 dir = v / dist;

		vec3 sample_color = light->color *scale  * dot(dir, l->face->plane.normal);
		
		l->pixel_colors[s] += sample_color;
	}
}

const float INDIRECT_SAMPLE_RADIUS = 3.0;

void CalculateIndirectLighting(LightmapState* l)
{
	std::vector<int> indicies;
	indicies.reserve(16);
	for (int s = 0; s < l->sample_points.size(); s++)
	{
		if (l->occluded[s])
			continue;
		patch_tree.PointsInRadius(l->sample_points[s], INDIRECT_SAMPLE_RADIUS, indicies);

		// Determine contribution

		vec3 total_indirect = vec3(0);
		float total_weight = 0;

		vec3 total_no_vis_indirect = vec3(0);
		float total_no_vis_weight = 0;
		for (int i = 0; i < indicies.size(); i++) {
			const patch_t* patch = &patches[patch_tree.points[indicies[i]].patch_index];
			vec3 patch_center = patch->center;
			vec3 patch_n = world->faces[patch->face].plane.normal;
			float angle = dot(patch_n, l->face->plane.normal);
			float dist = length(l->sample_points[s]- patch_center);
			float weight = ((INDIRECT_SAMPLE_RADIUS - dist))/INDIRECT_SAMPLE_RADIUS * angle;
			if (weight <= 0)
				continue;

			total_no_vis_weight += weight;
			total_no_vis_indirect += patch->total_light * weight;

			if (config.test_patch_visibility&& dist >= 0.05) {
				vec3 to_patch = patch_center-l->sample_points[s];
				float projected = dot(to_patch, l->face->plane.normal);
				if (projected >= 0) {
					// Simple LOS test
					auto res = global_world.tree.test_ray_fast(l->sample_points[s],patch_center);
					if (res.hit)
						continue;
				}
				else {
					// Patch is "below the horizon"
					vec3 a = projected * l->face->plane.normal;
					vec3 b = to_patch - a;	// orthagonal component
					
					float angle = dot(to_patch, patch_n);
					if (angle <= -0.1) continue;
					
					float test = dot(b, l->face->plane.normal);
					assert(abs(test) < 0.05);

					auto res = global_world.tree.test_ray_fast(l->sample_points[s],l->sample_points[s]+b);
					if (res.hit)
						continue;
				}
			}

			total_weight += weight;
			total_indirect += patch->total_light * weight;
		}
		if (total_weight > 0)
			l->pixel_colors[s] += total_indirect / total_weight;
		else {
			// This sometimes happens, just use the non-vised sum
			if (config.debug_indirect_occluded_samples)
				l->pixel_colors[s] = vec3(0, 0, 1);
			else if (total_no_vis_weight > 0)
				l->pixel_colors[s] += total_no_vis_indirect / total_no_vis_weight;
			else
				l->occluded[s] = 1;	// hopefully it gets dilated later
		}
	}
}

void light_face(int num)
{
	LightmapState l;
	Image img;
	assert(num < num_faces);
	l.face = &world->faces[num];
	l.surf_num = num;
	//l.fl = &facelights[num];
	img.face_num = num;

	const texture_info_t* ti = &world->t_info[l.face->t_info_idx];
	if (l.face->dont_draw)
		return;

	calc_vectors(l);
	calc_extents(l);

	calc_points(l, img);
	if(config.using_dirt)
		CalculateDirtMap(&l);

	if (!config.no_direct) {
		for (const auto& light : LightList()) {
			CalcFaceDirectLighting(&l, &light);
		}
	}

	if (config.enable_radiosity) {
		CalculateIndirectLighting(&l);
	}

	if (config.using_dirt || config.debug_dirt) {
		for (int s = 0; s < l.sample_points.size(); s++) {
			if (l.occluded[s])continue;
			float val = GetDirtValue(l.occlusion[s]);
			if (config.debug_dirt)
				l.pixel_colors[s] = vec3(val);
			else
				l.pixel_colors[s] *= val;
		}
	}


	FloodFillOccludedSamples(&l);
	BoxBlur(&l, 1);

	img.face_num = num;
	img.height = l.height;
	img.width = l.width;
	//img.buffer_start = data_buffer.size();
	//make_space(img.height* img.width);
	img.buffer_start = sample_buffer.MakeSpace(img.height*img.width);

	for (int y = 0; y < img.height; y++) {
		for (int x = 0; x < img.width; x++) {
			int index = y * img.width + x;

			Sample s;
			s.color = l.pixel_colors[index];
			s.normal = l.face->plane.normal;
			s.position = l.sample_points[index];
			s.edge = l.occluded[index] & OCCLUDER_EDGE;
			s.occluded = l.occluded[index] & IS_OCCLUDED;

			if (!s.occluded && (x == 0 || x == img.width - 1 || y == 0 || y == img.height - 1))
				s.edge = true;


			sample_buffer.AddSample(img, index, s);


		//	vec3 total = vec3(0);
		//	int added = 0;
		//	int offset = y * img.width + x;
		//	total = l.pixel_colors[y * img.width + x];
		//	total = glm::pow(total, vec3(1.0 / 2.2));
		//	add_color(img.buffer_start, offset, total);
		}
	}

	sample_buffer.AddImage(img);


	//images.push_back(img);

}



void add_to_buffer(int x, int y, u8 r, u8 g, u8 b)
{
	assert(x < lm_width&& y < lm_height);
	int start = y * lm_width * 3 + x * 3;
	final_lightmap.at(start) = r;
	final_lightmap.at(start + 1) = g;
	final_lightmap.at(start + 2) = b;
}

void SaveFloatAsRgb(vec3 in_rgb, uint8* r, uint8* g, uint8* b)
{
	vec3 rgb = pow(in_rgb, vec3(1 / 2.2));
	int r_ = rgb.x * 255;
	int g_ = rgb.y * 255;
	int b_ = rgb.z * 255;
	r_ = clamp(r_, 0, 255);
	g_ = clamp(g_, 0, 255);
	b_ = clamp(b_, 0, 255);

	*r = r_;
	*g = g_;
	*b = b_;
}

void append_to_lightmap(int x, int y, const Image& img)
{
	int x_coord = x;
	int y_coord = y;

	for (int y = 0; y < img.height; y++) {
		for (int x = 0; x < img.width; x++) {

			int temp_buf_offset = img.buffer_start + y * img.width + x;
			vec3 color = sample_buffer.samples.at(temp_buf_offset).color;
			u8 r, g, b;
			SaveFloatAsRgb(color, &r, &g, &b);

			add_to_buffer(x_coord + x, y_coord + y, r, g, b);
		}
	}

}


void PackRoverMethod()
{
	std::sort(sample_buffer.images.begin(), sample_buffer.images.end(), [](const Image& i1, const Image& i2)
		{ return i1.height > i2.height; });

	std::vector<int> allocated(lm_width, 0);
	for (int i = 0; i < sample_buffer.images.size(); i++) {
		Image* img = &sample_buffer.images.at(i);

		int rect_x{};
		int rect_y{};

		int x, x1;
		int best_height, best_tentative_height;
		best_height = lm_height;
		for (x = 0; x < lm_width - img->width; x++)
		{
			best_tentative_height = 0;
			for (x1 = 0; x1 < img->width; x1++)
			{
				if (allocated[x + x1] >= best_height)
					break;
				if (allocated[x + x1] > best_tentative_height)
					best_tentative_height = allocated[x + x1];
			}
			if (x1 == img->width)
			{
				rect_x = x;
				rect_y = best_height = best_tentative_height;
			}
		}
		if (best_height + img->height > lm_height)
		{
			printf("NOT ENOUGH SPACE IN LIGHTMAP\n");
			return;
		}

		for (int k = 0; k < img->width; k++)
			allocated[rect_x + k] = best_height + img->height;

		append_to_lightmap(rect_x, rect_y, sample_buffer.images.at(i));

		face_t* f = &world->faces[sample_buffer.images.at(i).face_num];
		f->lightmap_min[0] = rect_x;
		f->lightmap_min[1] = rect_y;

		f->lightmap_size[0] = img->width;
		f->lightmap_size[1] = img->height;
	}
}


void mark_bad_faces()
{
	const brush_model_t* bm = &world->models[0];
	for (int i = bm->face_start; i < bm->face_start + bm->face_count; i++) {
		face_t* f = &world->faces[i];
		vec3 middle = vec3(0.f);
		for (int v = 0; v < f->v_count; v++) {
			middle += world->verts[f->v_start+v];
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

struct SamplePointType
{
	int index{};
	vec3 original_color;
	vec3 GetPoint() const {
		return sample_buffer.samples[index].position;
	}
};

void FixSeams()
{
	PointTree<SamplePointType> sample_tree;

	for (int i = 0; i < sample_buffer.samples.size(); i++) {
		if (!sample_buffer.samples[i].edge)
			continue;
		SamplePointType spt;
		spt.index = i;
		spt.original_color = sample_buffer.samples[i].color;
		sample_tree.points.push_back(spt);
	}
	printf("Fixing seams, num edge points: %d\n", (int)sample_tree.points.size());
	sample_tree.Build();

	const float radius = 2 / config.pixel_density;
	const float radius_2 = radius * radius;
	const float angle_max = 0.9;
	std::vector<int> indicies;
	for (int i = 0; i < sample_tree.points.size(); i++) {
		const SamplePointType& spt = sample_tree.points[i];
		Sample& sample = sample_buffer.samples[spt.index];

		sample_tree.PointsInRadius(sample.position, radius, indicies);

		vec3 total_sum = vec3(0);
		float total_weight = 0;
		for (int i = 0; i < indicies.size(); i++) {
			SamplePointType overlap_spt = sample_tree.points[indicies[i]];
			Sample overlap_sample = sample_buffer.samples[overlap_spt.index];

			float dist_2 = dot(overlap_sample.position - sample.position, overlap_sample.position - sample.position);
			float angle = dot(sample.normal, overlap_sample.normal);
			angle = (angle < angle_max) ? 0 : 1.f;

			float weight = (radius_2 - dist_2)*angle;

			if (weight >= 0) {
				total_sum += overlap_spt.original_color * weight;
				total_weight += weight;
			}
		}

		if (total_weight > 0) {
			total_sum /= total_weight;
			sample.color = total_sum;
		}
		else {
			sample.color = vec3(1, 0, 0);
		}
	}
}
#include <array>
void DilateEdges()
{
	const int SEARCH_RADIUS = 4;
	const int SEARCH_WIDTH = SEARCH_RADIUS * 2 + 1;
	// Too dumb to think of a smart way to do this
	std::array<ivec2, SEARCH_WIDTH*SEARCH_WIDTH> nums;
	for (int y = 0; y < SEARCH_WIDTH; y++) {
		for (int x = 0; x < SEARCH_WIDTH; x++) {
			nums[y * SEARCH_WIDTH + x] = { x - SEARCH_RADIUS,y - SEARCH_RADIUS };
		}
	}
	std::sort(nums.begin(), nums.end(),
		[](const ivec2& i, const ivec2& i2)
		{ return dot(vec2(i), vec2(i)) < dot(vec2(i2), vec2(i2)); });


	for (int i = 0; i < sample_buffer.images.size(); i++) {
		const Image& img = sample_buffer.images[i];
		for (int y = 0; y < img.height; y++) {
			for (int x = 0; x < img.width; x++) {
				const int index = img.buffer_start + y * img.width + x;
				
				if (!sample_buffer.samples[index].occluded) 
					continue;
				// n=0 is just (0,0)
				for (int n = 1; n < nums.size(); n++) {
					const int x1 = x + nums[n].x;
					const int y1 = y + nums[n].y;
					if (x1 < 0 || x1 >= img.width || y1 < 0 || y1 >= img.height)
						continue;
					const int new_index = img.buffer_start + y1 * img.width + x1;
					if (!sample_buffer.samples[new_index].occluded) {
						// Found good sample
						sample_buffer.samples[index].color = sample_buffer.samples[new_index].color;
						break;
					}
				}
			}
		}
	}
}

const int TRANSFER_VERSION = 1;
void SaveTransferFile()
{
	std::ofstream out("resources/maps/" + world->name + ".rad",std::ios::binary);
	if (!out) {
		printf("Couldn't write transfers\n");
		return;
	}

	out.write("RAD", 4);
	out.write((char*)&TRANSFER_VERSION, sizeof(int));
	
	const int patches_num = patches.size();
	out.write((char*)&patches_num, sizeof(int));

	for (int i = 0; i < patches.size(); i++) {
		const auto& p = patches[i];
		out.write((char*)&p.sky_visibility, sizeof(float));
		out.write((char*)&p.num_transfers, sizeof(int));
		if (p.num_transfers > 0) {
			out.write((char*)p.transfers, sizeof(transfer_t) * p.num_transfers);
		}
	}
	size_t bytes_written = out.tellp();
	out.close();
}
bool ReadTransferFile()
{
	std::ifstream in("resources/maps/" + world->name + ".rad", std::ios::binary);
	if (!in) {
		printf("Couldn't read transfer file\n");
		return false;
	}
	char buffer[4];

	in.read(buffer,4);
	if (buffer[0] != 'R' || buffer[1] != 'A' || buffer[2] != 'D') {
		printf("Couldn't recognize file\n");
		return false;
	}

	int transfer_version = 0;
	in.read((char*)&transfer_version, sizeof(int));

	if (transfer_version != TRANSFER_VERSION) {
		printf("Outdated .rad version\n");
		return false;
	}

	int file_patches = 0;
	in.read((char*)&file_patches, sizeof(int));

	if (file_patches != patches.size()) {
		printf(".rad patch size mismatch, skipping\n");
		return false;
	}

	for (int i = 0; i < patches.size(); i++) {
		auto& p = patches[i];
		in.read((char*)&p.sky_visibility, sizeof(float));
		in.read((char*)&p.num_transfers, sizeof(int));
		if (p.num_transfers > patches.size()) {
			printf("Corrupted transfer file\n");
			exit(-1);
		}
		if (p.num_transfers>0) {
			p.transfers = new transfer_t[p.num_transfers];
			in.read((char*)p.transfers, sizeof(transfer_t) * p.num_transfers);
		}
	}
	printf("Restored patch transfers from .rad file\n");
	in.close();
	return true;
}

void create_light_map(worldmodel_t* wm, LightmapSettings settings)
{
	config = settings;
	config.using_dirt = true;
	config.no_direct = true;
	config.enable_radiosity = true;
	config.dirt_gain = 3.0;
	config.dirt_dist = 5.0;
	config.num_dirt_vectors = 25;
	config.test_patch_visibility = true;
	config.debug_indirect_occluded_samples=false;
	config.debug_dirt = false;
	config.default_reflectivity = vec3(0.5);
	GetSky().sky_color = RGBToFloat(200, 138, 0);


	world = wm;
	va.init(VertexArray::Primitive::LINES);
	point_array.init(VertexArray::Primitive::POINTS);
	voxels.init(VertexArray::TRIANGLES);

	// Used for random debug colors
	srand(time(NULL));

	u32 start = SDL_GetTicks();
	printf("Starting lightmap...\n");

	brush_tree.Build(wm,0,wm->map_brushes.size());
	
	AddLightEntities(wm);

	if (config.inside_map) {
		mark_bad_faces();
	}

	if (config.enable_radiosity) {
		MakePatches();
		CreatePatchTree();
		printf("Total patches: %d\n", (int)patches.size());
	

		transfers.resize(patches.size());
		if (!ReadTransferFile()) {
			printf("Making transfers...\n");
			for (int i = 0; i < patches.size(); i++)
				MakeTransfers(i);
			printf("Finished transfers\n");
		}
		//run_threads_on_function(&MakeTransfers, 0, patches.size());
		printf("Bouncing light...\n");
		RadWorld();
	}

	printf("Lighting faces...\n");
	const brush_model_t* bm = &wm->models[0];	// "worldspawn"
	for (int i = bm->face_start; i < bm->face_start + bm->face_count; i++) {
		light_face(i);
	}
	//run_threads_on_function(&light_face, bm->face_start, bm->face_start + bm->face_count);

	printf("Total pixels: %u\n", total_pixels);
	printf("Total raycasts: %u\n", total_rays_cast);


	printf("Writing output...\n");
	final_lightmap.resize(lm_width * lm_height * 3, 0);
	for (int i = 0; i < final_lightmap.size(); i += 3) {
		final_lightmap[i] = 255;
		final_lightmap[i+1] = 0;
		final_lightmap[i+2] = 255;

	}
	FixSeams();
	DilateEdges();

	PackRoverMethod();
	stbi_write_bmp(("resources/textures/" + wm->name + "_lm.bmp").c_str(), lm_width, lm_height, 3, final_lightmap.data());
	
	printf("Lightmap finished in %u ms\n", SDL_GetTicks() - start);
	
	print_rays_cast();
	create_patch_view(PatchDebugMode::RADCOLOR);
	
	if (config.enable_radiosity) {
		SaveTransferFile();
	}
	
	FreePatches();
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