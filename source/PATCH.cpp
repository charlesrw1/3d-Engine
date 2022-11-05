#include "Light.h"
#include "WorldGeometry.h"
#include <string>
std::vector<patch_t> patches;
std::vector<int> face_patches;

static void PatchForFace(int face_num)
{
	const face_t* face = &GetWorld()->faces[face_num];
	const texture_info_t* ti = &GetWorld()->t_info[face->t_info_idx];
	const std::string* t_string = &GetWorld()->texture_names[ti->t_index];
	// Skybox doesn't make patches
	if (face->dont_draw || ti->flags & SURF_EMIT) {
		return;
	}
	const int patch_index = patches.size();
	patch_t p{};
	p.self_index = patch_index;
	p.face = face_num;
	face_patches[face_num] = patch_index;

	for (int i = 0; i < face->v_count; i++) {
		p.winding.add_vert(GetWorld()->verts[face->v_start + i]);
	}
	p.area = p.winding.get_area();
	p.area = max(p.area, 0.1f);
	p.center = p.winding.get_center();


	if (*t_string == "color/red") {
		p.reflectivity = RGBToFloat(237, 28, 36);
	}
	else if (*t_string == "color/green") {
		p.reflectivity = RGBToFloat(34, 177, 76);
	}
	else if (*t_string == "color/blue") {
		p.reflectivity = RGBToFloat(0, 128, 255);
	}
	else if (*t_string == "color/yellow") {
		p.reflectivity = RGBToFloat(255, 242, 0);
	}
	else {
		auto rflc_idx = t_string->find("dev/rflc");
		if (rflc_idx != std::string::npos) {
			int brightness = std::stoi(t_string->substr(t_string->size() - 2, 2));
			p.reflectivity = vec3(brightness / 100.0);
		}
		else {
			p.reflectivity = config.default_reflectivity;
		}
	}
	patches.push_back(p);
	p.next = -1;
}

void MakePatches()
{
	face_patches.resize(GetWorld()->faces.size());
	const brush_model_t* bm = &GetWorld()->models[0];
	for (int i = bm->face_start; i < bm->face_start + bm->face_count; i++) {
		PatchForFace(i);
	}
}

static int failed_subdivide = 0;
void SubdividePatch(int patch_num)
{
	patch_t* p = &patches[patch_num];
	vec3 min, max;
	winding_t front, back;
	get_extents(p->winding, min, max);
	int i;
	for (i = 0; i < 3; i++) {
		// Quake 2 had the patch grid as an actual grid thats not relative to a specific patch
		// However this leads to some really small patches, more matches no matter the size tank performance (n^2)
		// Instead make it relative to the face
		if (min[i] + config.patch_grid + (config.patch_grid) <= max[i]) {
			plane_t split;
			split.normal = vec3(0);
			split.normal[i] = 1.f;
			split.d = -(min[i] + config.patch_grid);
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

	///assert(num_patches < MAX_PATCHES);
	const int new_patch_index = patches.size();
	patch_t new_p;
	new_p.self_index = new_patch_index;

	new_p.next = p->next;
	p->next = new_p.self_index;

	p->winding = front;
	new_p.winding = back;

	new_p.face = p->face;

	p->center = p->winding.get_center();
	new_p.center = new_p.winding.get_center();
	p->area = p->winding.get_area();
	new_p.area = new_p.winding.get_area();

	new_p.reflectivity = p->reflectivity;

	new_p.sample_light = p->sample_light;
	new_p.total_light = p->total_light;
	new_p.emission = p->emission;

	patches.push_back(new_p);

	SubdividePatch(patch_num);
	SubdividePatch(new_patch_index);

}
void SubdividePatches()
{
	int num = patches.size();	// patches will grow in size
	for (int i = 0; i < num; i++) {
		SubdividePatch(i);
	}
	printf("Failed num: %d\n", failed_subdivide);
}
