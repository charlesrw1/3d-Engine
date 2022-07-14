#include "Voxelizer.h"
#include "Map_def.h"
#include "opengl_api.h"
#include <random>
Voxelizer::Voxelizer(const worldmodel_t* wm,float voxel_size,int bundle_size) :wm(wm),voxel_size(voxel_size),bundle_size(bundle_size)
{
	if (bundle_size > MAX_BUNDLE_SIZE)
		bundle_size = MAX_BUNDLE_SIZE;

	compute_bounds();
	voxelize();
	printf("Total solid voxels: %d\n", total_solid);
	flood_fill_outside();
	printf("Total outside voxels: %d\n", total_outdoors);
	create_samples();
	printf("Total samples from voxels: %d\n", (int)final_sample_spots.size());
}

void Voxelizer::compute_bounds()
{
	vec3 min=vec3(8000), max=vec3(-8000);
	const brush_model_t* bm = &wm->models[0];
	for (int i = bm->face_start; i < bm->face_start + bm->face_count; i++) {
		const face_t* f = &wm->faces[i];
		for (int j = 0; j < f->v_count; j++) {
			vec3 v = wm->verts[f->v_start + j];
			for (int m = 0; m < 3; m++) {
				if (v[m] < min[m])
					min[m] = v[m];
				if (v[m] > max[m])
					max[m] = v[m];
			}
		}
	}

	map_min = min;
	map_size = max - min;

}
/*
	// loop over voxels
		ivec3 voxel_start, voxel_count;
		voxel_start = glm::floor((min - map_min) / voxel_size);
		voxel_start -= ivec3(1);
		voxel_count = glm::ceil((max - min) / voxel_size);
		voxel_count += ivec3(1);
		voxel_start = glm::max(voxel_start, ivec3(0));
		for (int z = 0; z < voxel_count.z; z++) {
			for (int y = 0; y < voxel_count.y; y++) {
				for (int x = 0; x < voxel_count.x; x++) {

					ivec3 loc = voxel_start + ivec3(x, y, z);
					loc = glm::min(loc, grid - ivec3(1));
*/
void Voxelizer::voxelize()
{
	grid = glm::ceil(map_size/voxel_size);
	grid_start = glm::floor(map_min);

	printf("Grid size: %d %d %d\n", grid.x, grid.y, grid.z);
	voxels.resize(grid.z * grid.y * grid.x);

	plane_t planes[6];
	planes[0].normal = vec3(1, 0, 0);
	planes[1].normal = vec3(-1, 0, 0);
	planes[2].normal = vec3(0, 1, 0);
	planes[3].normal = vec3(0, -1, 0);
	planes[4].normal = vec3(0, 0, 1);
	planes[5].normal = vec3(0, 0, -1);

	total_solid = 0;


	// Intersect all faces with voxels
	const brush_model_t* bm = &wm->models[0];
	for (int i = bm->face_start; i < bm->face_start + bm->face_count; i++) {
		const face_t* f = &wm->faces[i];
		winding_t w;
		// load into a winding for ease of use
		for (int j = 0; j < f->v_count; j++) {
			vec3 v = wm->verts[f->v_start + j];
			w.add_vert(v);
		}
		if (i == 116 || i==617) {
			printf("");
		}

		vec3 min, max;
		get_extents(w, min, max);
		unsigned char r, g, b;
		r = rand() % 256;
		g = rand() % 256;
		b = rand() % 256;

		// loop over voxels
		ivec3 voxel_start, voxel_count;
		voxel_start = glm::floor(min - map_min)/voxel_size;
		voxel_count = glm::ceil((glm::ceil(max) - glm::floor(min)) / voxel_size);
		voxel_count = glm::max(voxel_count, ivec3(1));
		voxel_count += ivec3(1);
		for (int m = 0; m < 3; m++) {
			if (voxel_count[m] + voxel_start[m] >= grid[m])
				voxel_count[m] = grid[m] - voxel_start[m];
		}
		for (int z = 0; z < voxel_count.z; z++) {
			for (int y = 0; y < voxel_count.y; y++) {
				for (int x = 0; x < voxel_count.x; x++) {
					Voxel& v = get_voxel(voxel_start + ivec3(x, y, z));
					if (v.flags & Voxel::SOLID) {
						continue;
					}
					vec3 vox_corner = vec3(grid_start) + vec3(voxel_start + ivec3(x, y, z))*voxel_size;
					vec3 vox_cor_max = vox_corner + vec3(voxel_size);
					
					int j;
					for (j = 0; j < 3; j++) {
						if (min[j] > vox_cor_max[j])
							break;
						if (max[j] < vox_corner[j])
							break;
					}
					// voxel and face boxes don't intersect
					if (j != 3)
						continue;

					vec3 corners[8];
					corners[0] = vox_corner;	//-x -y -z
					corners[1] = vec3(vox_corner.x + voxel_size,vox_corner.y,vox_corner.z);// x -y -z
					corners[2] = vec3(vox_corner.x + voxel_size, vox_corner.y + voxel_size, vox_corner.z);// x y -z
					corners[3] = vec3(vox_corner.x, vox_corner.y + voxel_size, vox_corner.z);// -x y -z

					corners[4] = vec3(vox_corner.x,vox_corner.y,vox_corner.z+voxel_size);	//-x -y z
					corners[5] = vec3(vox_corner.x + voxel_size, vox_corner.y, vox_corner.z+voxel_size);// x -y z
					corners[6] = vec3(vox_corner.x + voxel_size, vox_corner.y + voxel_size, vox_corner.z + voxel_size);// x y z
					corners[7] = vec3(vox_corner.x, vox_corner.y + voxel_size, vox_corner.z + voxel_size);// -x y z
					// check box corners with face plane
					bool front = false;
					bool back = false;
					for (j = 0; j < 8; j++) {
						float dist = f->plane.distance(corners[j]);
						if (dist < 0.01)
							back = true;
						if (dist > -0.01)
							front = true;

						if (front && back)
							break;
					}
					// verticies were all on one side of the plane, no intersection
					if (!front || !back)
						continue;
					// Voxel intersects face
					total_solid++;
					v.flags |= Voxel::SOLID;
					v.r = r; v.g = g; v.b = b;
					next_voxel:;
				}
			}
		}
	}
}

void Voxelizer::flood_fill_outside()
{
	int x1;
	bool span_above_y, span_below_y,span_above_z,span_below_z;
	std::vector<ivec3> stack;
	stack.push_back(ivec3(0));
	stack.push_back(ivec3(grid)-ivec3(1));

	while (!stack.empty())
	{
		ivec3 pos = stack.back();
		stack.pop_back();
		x1 = pos.x;
		while (x1 >= 0) {
			Voxel& v = get_voxel(ivec3(x1, pos.y, pos.z));
			if (v.flags & Voxel::SOLID) break;
			x1--;
		}
		x1++;

		span_above_y = false;
		span_below_y = false;

		span_above_z = false;
		span_below_z = false;

		while (x1 < grid.x) {
			Voxel& v = get_voxel(ivec3(x1, pos.y, pos.z));
			if (v.flags & Voxel::SOLID || v.flags & Voxel::OUTSIDE)
				break;
			v.flags |= Voxel::OUTSIDE;
			total_outdoors++;


			// Y
			if (!span_below_y && pos.y > 0) {
				Voxel& ybelow = get_voxel(ivec3(x1, pos.y - 1, pos.z));
				if (!(ybelow.flags & Voxel::SOLID)) {	// Non solid voxel
					span_below_y = true;
					stack.push_back(ivec3(x1, pos.y - 1, pos.z));
				}
			}
			else if (span_below_y && pos.y > 0) {
				Voxel& ybelow = get_voxel(ivec3(x1, pos.y - 1, pos.z));
				if (ybelow.flags & Voxel::SOLID)	// Solid voxel, potentially another span ahead
					span_below_y = false;
			}
			if (!span_above_y && pos.y < grid.y - 1) {
				Voxel& yabove = get_voxel(ivec3(x1, pos.y + 1, pos.z));
				if (!(yabove.flags & Voxel::SOLID)) {
					span_above_y = true;
					stack.push_back(ivec3(x1, pos.y + 1, pos.z));
				}
			}
			else if (span_above_y && pos.y < grid.y - 1) {
				Voxel& yabove = get_voxel(ivec3(x1, pos.y + 1, pos.z));
				if (yabove.flags & Voxel::SOLID)
					span_above_y = false;
			}

			// Z
			if (!span_below_z && pos.z > 0) {
				Voxel& zbelow = get_voxel(ivec3(x1, pos.y, pos.z - 1));
				if (!(zbelow.flags & Voxel::SOLID)) {	// Non solid voxel
					span_below_z = true;
					stack.push_back(ivec3(x1, pos.y, pos.z - 1));
				}
			}
			else if (span_below_z && pos.z > 0) {
				Voxel& zbelow = get_voxel(ivec3(x1, pos.y, pos.z - 1));
				if (zbelow.flags & Voxel::SOLID)	// Solid voxel, potentially another span ahead
					span_below_z = false;
			}
			if (!span_above_z && pos.z < grid.z - 1) {
				Voxel& zabove = get_voxel(ivec3(x1, pos.y, pos.z+1));
				if (!(zabove.flags & Voxel::SOLID)) {
					span_above_z = true;
					stack.push_back(ivec3(x1, pos.y, pos.z+1));
				}
			}
			else if (span_above_z && pos.z < grid.z - 1) {
				Voxel& zabove = get_voxel(ivec3(x1, pos.y, pos.z+1));
				if (zabove.flags & Voxel::SOLID)
					span_above_z = false;
			}




			x1++;
		}

	}

}
void Voxelizer::create_samples()
{
	Voxel* voxels[MAX_BUNDLE_SIZE*MAX_BUNDLE_SIZE*MAX_BUNDLE_SIZE];
	memset(voxels, 0, sizeof(voxels));
	vec3 vox_corner;
	int bundle_sq = bundle_size * bundle_size;
	int bundle_cube = bundle_size * bundle_size*bundle_size;


	for (int z = 0; z < grid.z-bundle_size+1; z++) {
		for (int y = 0; y < grid.y-bundle_size+1; y++) {
			for (int x = 0; x < grid.x-bundle_size+1; x++) {

				for (int z1 = 0; z1 < bundle_size; z1++) {
					for (int y1 = 0; y1 < bundle_size; y1++) {
						for (int x1 = 0; x1 < bundle_size; x1++) {
							Voxel& v = get_voxel(ivec3(x + x1, y + y1, z + z1));

							if (v.flags & Voxel::OUTSIDE || v.flags & Voxel::SOLID || v.flags & Voxel::USED_FOR_AMBIENT)
								goto skip_voxel;

							voxels[z1 * bundle_sq + y1 * bundle_size + x1] = &v;
						}
					}
				}
				for (int i = 0; i < bundle_cube; i++) {
					voxels[i]->flags |= Voxel::USED_FOR_AMBIENT;
				}
				vox_corner = vec3(grid_start) + vec3(ivec3(x,y,z)) * voxel_size;
				final_sample_spots.push_back(vox_corner + vec3(voxel_size));

				skip_voxel:;
			}
		}
	}
}
void Voxelizer::add_points(VertexArray* va)
{
	/*
	for (int z = 0; z < grid.z; z++) {
		for (int y = 0; y < grid.y; y++) {
			for (int x = 0; x < grid.x; x++) {
				Voxel& v = get_voxel(ivec3(x, y, z));
				if ((v.flags & Voxel::OUTSIDE || v.flags & Voxel::SOLID)) 
					continue;
				//if (!(v.flags & Voxel::SOLID))
				//	continue;
				vec3 min = vec3(grid_start) + vec3(x, y, z) * voxel_size;
				vec3 max = min + vec3(voxel_size);

				va->add_solid_box(min, max, vec4(x/(float)grid.x, y / (float)grid.y, z / (float)grid.z, 1.0));

			}
		}
	}
	*/

	for (int i = 0; i < final_sample_spots.size(); i++) {
		va->add_solid_box(final_sample_spots[i] - vec3(voxel_size * 0.5), final_sample_spots[i] + vec3(voxel_size * 0.5),vec4((final_sample_spots[i]-map_min)/map_size,1.0));
	}

}