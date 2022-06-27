#include "Light.h"
#include "WorldGeometry.h"
#include "SDL2/SDL_timer.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

face_t* faces;
int num_faces;

const vec3* verts;
int num_verts;

texture_info_t* tinfo;
int num_tinfo;


std::vector<u8> data_buffer;

std::vector<u8> final_lightmap;

int lm_width = 1800;
int lm_height = 1800;
// how many texels per 1.0 meters/units
// 32 quake units = 1 my units
float density_per_unit = 4.f;

VertexArray va;

vec3 lerp(const vec3& a, const vec3& b, float percentage)
{
	return a + (b - a) * percentage;
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

const int MAP_PTS = 256 * 256;
std::vector<vec3> points(MAP_PTS);
std::vector<vec3> temp_image_buffer(MAP_PTS);
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
	total_pixels += l.numpts;

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
				goto face_too_big;
			}
			assert(top_point < MAP_PTS);
			points[top_point++] = point;
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

	img.face_num = num;

	calc_vectors(l);
	calc_extents(l);

	vec3 face_mid = l.face_middle + l.face->plane.normal * 0.02f;
	trace_t test_if_outside_map = global_world.tree.test_ray_fast(face_mid, face_mid + l.face->plane.normal * 100.f);
	total_rays_cast++;

	if (!test_if_outside_map.hit) {
		l.face->dont_draw = true;
		return;
	}
	calc_points(l, img);
	{
		img.buffer_start = data_buffer.size();
		make_space(img.height * img.width);
		// ambient term
		for (int i = 0; i < l.numpts; i++) {
			temp_image_buffer[i] = vec3(0);
			//add_color(img.buffer_start, i, vec3(0.05, 0.05, 0.05));
		}

		for (int i = 0; i < lights.size(); i++) {
			vec3 light_p = lights[i].pos;

			float dist = l.face->plane.distance(light_p);
			if (dist < 0) {
				continue;
			}

			for (int j = 0; j < l.numpts; j++) {

				float sqrd_dist = dot(light_p - points[j], light_p - points[j]);
				if (sqrd_dist > lights[i].brightness * lights[i].brightness) {
					continue;
				}

				total_rays_cast++;
				trace_t res = global_world.tree.test_ray_fast(points[j], light_p);
				if (res.hit) {
					continue;
				}
				//va->append({ l.points[j],vec3(0,1,0) });

				vec3 light_dir = light_p - points[j];
				float dist = length(light_dir);
				light_dir = normalize(light_dir);
				float dif = dot(light_dir, l.face->plane.normal);
				if (dif < 0) {
					continue;
				}

				vec3 final_color = lights[i].color * dif * max(1 / (dist * dist + lights[i].brightness) * (lights[i].brightness - dist), 0.f);
				temp_image_buffer[j] += final_color;
				//temp_image_buffer[j] = min(temp_image_buffer[j], vec3(1));
				//add_color(img.buffer_start, j, final_color);
			}
		}
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
	
	
	

	images.push_back(img);
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

#include <algorithm>
void create_light_map(worldmodel_t* wm)
{
	u32 start = SDL_GetTicks();
	printf("Starting lightmap...\n");
	va.init(VertexArray::Primitive::LINES);

	faces = wm->faces.data();
	num_faces = wm->faces.size();

	verts = wm->verts.data();
	num_verts = wm->verts.size();

	tinfo = wm->t_info.data();
	num_tinfo = wm->t_info.size();

	add_lights(wm);

	u32 start_light_face = SDL_GetTicks();
	printf("Lighting faces...\n");
	const brush_model_t* bm = &wm->models[0];	// "worldspawn"
	for (int i = bm->face_start; i < bm->face_start + bm->face_count; i++) {
		light_face(i);
	}
	printf("Total pixels: %u\n", total_pixels);
	printf("Total raycasts: %u\n", total_rays_cast);
	printf("Face lighting time: %u\n", SDL_GetTicks() - start_light_face);

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
	printf("Lightmap finished in %u ms", SDL_GetTicks() - start);
}
void draw_lightmap_debug()
{
	va.draw_array();
}