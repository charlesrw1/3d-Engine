#include "Light.h"
#include <vector>


// This is similar to what Valve uses in VRAD, Quake 2's triangulation method was garbage frankly
class Radial
{
public:
	Radial(facelight_t* fl, face_t* face) : fl(fl), face(face) {
		weights = new float[fl->num_pixels];
		memset(weights, 0, fl->num_pixels * sizeof(float));
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
		vec3 face_middle = vec3(0.0f);
		vec3 face_min, face_max;
		for (int i = 0; i < face->v_count; i++) {
			face_winding.add_vert(verts[face->v_start + i]);
			face_middle += verts[face->v_start + i];
		}
		face_middle /= face->v_count;
		get_extents(face_winding, face_min, face_max);

		// Loop through all patches on the plane that the face is on
		pn = &planes[index_to_plane_list[face_num]];
#if 1
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
					if (patch_max[i] + patch_grid < face_min[i])
						break;
					if (patch_min[i] - patch_grid > face_max[i])
						break;
				}
				if (i == 3) {
					// prevents some light bleeding, some make it through though
					trace_t check_wall_collison;
					check_wall_collison.hit = false;
					// vec3 compare fixes cases where center and face middle are the same, if so, dont cast a ray
					if (test_patch_visibility && !vec3_compare(p->center, face_middle, 0.01)) {
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
#endif


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
				if (point[j]<ext_min[j] - 0.1f || point[j]>ext_max[j] + 0.1f)
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


struct facelight_t
{
	int num_points = 0;
	vec3* sample_points = nullptr;

	int num_pixels = 0;
	vec3* pixel_colors = nullptr;

	int width = 0, height = 0;
};

facelight_t facelights[0x10000];

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

	facelight_t* fl = nullptr;

	// For supersampling, final facelight_t points are the center of the 4 (they might get shifted around)
	std::vector<vec3> sample_points[4];
	int num_samples = 4;

	int surf_num = 0;
	face_t* face = nullptr;
};
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
	step[0] = (l.exact_max[0] - l.exact_min[0]) / (w - 1);
	step[1] = (l.exact_max[1] - l.exact_min[1]) / (h - 1);

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

vec3 gather_standard(const light_t* light, vec3 sample, vec3 face_normal)
{
	float dist = length(light->pos - sample);
	vec3 dir = (light->pos - sample) / dist;
	float dot = glm::dot(face_normal, dir);
	if (dot <= 0)
		return vec3(0);
	float scale = 1.f;
	bool skip = true;
	if (light->type == LIGHT_POINT) {
		scale = (1 - dist / light->brightness) * dot;
		skip = scale < 0;
	}
	else if (light->type == LIGHT_SPOT) {
		float cos_theta = glm::dot(-dir, light->normal);
		scale = (1 - dist / light->brightness) * dot;
		skip = cos_theta - light->width < 0 || scale < 0;
	}
	if (skip)
		return vec3(0);
	total_rays_cast++;
	trace_t res = global_world.tree.test_ray_fast(sample, light->pos, -0.005f, 0.005f);
	if (res.hit)
		return vec3(0);
	return light->color * scale;
}
vec3 gather_sun(const light_t* light, vec3 sample, vec3 face_normal)
{
	float angle = dot(face_normal, -light->normal);
	if (angle <= 0)
		return vec3(0);
	total_rays_cast++;
	trace_t res = global_world.tree.test_ray_fast(sample, sample + (-light->normal * 300.f), -0.005f, 0.005f);
	// If it traces to the sun, it should hit a skybox face
	if (res.hit) {
		face_t* face = faces + res.face;
		texture_info_t* ti = tinfo + face->t_info_idx;
		if (!(ti->flags & SURF_SKYBOX))
			return vec3(0);
	}

	return light->color * angle * light->brightness;
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

				if (light->type == LIGHT_POINT || light->type == LIGHT_SPOT)
					final_color = gather_standard(light, sample_pos, l.face->plane.normal);
				else if (light->type == LIGHT_SUN)
					final_color = gather_sun(light, sample_pos, l.face->plane.normal);

				l.fl->pixel_colors[j] += final_color * sample_scale;
#if 0

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

					float extra_scale = 1;
					if (light->type == LIGHT_SPOT) {
						float cos_theta = dot(-light_dir, light->normal);
						float outer = light->width * 0.90;
						float epsilon = light->width - outer;
						extra_scale = clamp((cos_theta - outer) / epsilon, 0.0f, 1.0f);
					}

					final_color = lights[i].color * dif * max((lights[i].brightness - dist), 0.f) * extra_scale;
#if 0
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
					float attenuation_factor = ((float)lights[i].brightness / (dist * dist + 0.01)) * (1 - dist / (float)lights[i].brightness);
					//max(1 / (dist * dist + lights[i].brightness) * (lights[i].brightness - dist), 0.f)
					final_color = lights[i].color * dif * attenuation_factor * extra_scale;
#endif
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
						float attenuation_factor = max(((float)lights[i].brightness / (dist * dist + 0.01)) * (1 - dist / (float)lights[i].brightness), 0.0);
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

				l.fl->pixel_colors[j] += final_color * sample_scale;
#endif
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
		Radial radial(fl, face);
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
