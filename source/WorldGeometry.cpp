#include "WorldGeometry.h"
#include "TextureManager.h"
#include "Texture_def.h"
#include "ModelManager.h"
#include "glad/glad.h"

WorldGeometry global_world;

void WorldGeometry::upload_map_to_tree()
{
	tree.clear();
	tree.init(wm);
	tree.create_va();
}
void WorldGeometry::init_render_data()
{
	model = new Model;
	line_va.init(VAPrim::LINES);
	hit_faces.init(VAPrim::LINES);
	hit_points.init(VAPrim::POINTS);
}
void WorldGeometry::create_mesh()
{

	std::vector<LightmapVert> gpu_verts;
	std::vector<u32> elements;

	std::vector<ivec2> texture_sizes;
	// first load textures, non-async because width/height is needed for uv coords
	for (const auto& t : wm->texture_names) {
		face_textures.push_back(global_textures.find_or_load((t + ".png").c_str(), TParams::LOAD_NOW | TParams::GEN_MIPS));
		if (face_textures.back()->is_loaded()) {
			texture_sizes.push_back(face_textures.back()->get_dimensions());
		}
		else texture_sizes.push_back(ivec2(256, 256));
	}
	const brush_model_t* bm = &wm->models[0];	// "worldspawn"
	std::vector<u32> indicies(bm->face_count);
	for (int i = 0; i < indicies.size(); i++) {
		indicies.at(i) = i+bm->face_start;
	}

	// sort faces by texture
	std::sort(indicies.begin(), indicies.end(),
		[this](const int& i1, const int& i2) -> bool
		{ return wm->t_info.at(wm->faces.at(i1).t_info_idx).t_index < wm->t_info.at(wm->faces.at(i2).t_info_idx).t_index; });

	int current_t = 0;
	for (int i =0; i < bm->face_count; i++) {
		auto& face = wm->faces.at(indicies.at(i));
		const texture_info_t* ti = &wm->t_info.at(face.t_info_idx);
		if (face.dont_draw || ti->flags & SURF_SKYBOX)
			continue;

		// We have a new texture, append previous mesh
		if (current_t != wm->t_info.at(face.t_info_idx).t_index) {
			if (!gpu_verts.empty()) {
				RenderMesh rm;
				rm.diffuse = face_textures.at(current_t);
				if (!rm.diffuse->is_loaded()) rm.diffuse = nullptr;
				M_upload_lightmap_mesh(&rm, gpu_verts.data(), elements.data(), gpu_verts.size(), elements.size());

				gpu_verts.clear();
				elements.clear();

				model->append_mesh(rm);
			}
			current_t = wm->t_info.at(face.t_info_idx).t_index;
		}
		
		// Generate verts for GPU
		int first_vert_index = gpu_verts.size();

		// Lightmap data
		// recomputed data from lightmapper functions
		vec3 tex_normal = normalize(cross(ti->axis[1], ti->axis[0]));
		float ratio = dot(tex_normal, face.plane.normal);
		if (ratio < 0) {
			ratio = -ratio;
			tex_normal = -tex_normal;
		}
		vec3 origin = vec3(0) + normalize(cross(ti->axis[1], ti->axis[0])) * (-face.plane.d / ratio);

		for (int j = 0; j < face.v_count; j++) {
			LightmapVert wrv;
			wrv.normal = normalize(face.plane.normal);
			wrv.position = wm->verts.at(face.v_start + j);

			texture_info_t* ti = &wm->t_info.at(face.t_info_idx);

			// Texture coordinates
			wrv.texture_uv.x = dot(wrv.position * 32.f, ti->axis[0]) / texture_sizes.at(ti->t_index).x / ti->uv_scale.x + ti->offset[0] / (float)texture_sizes.at(ti->t_index).x;
			wrv.texture_uv.y = dot(wrv.position * 32.f, ti->axis[1]) / texture_sizes.at(ti->t_index).y / ti->uv_scale.y + ti->offset[1] / (float)texture_sizes.at(ti->t_index).y;
			if (abs(wrv.texture_uv.x) > 50) {
				wrv.texture_uv.x = 0;
			}
			if (abs(wrv.texture_uv.y) > 50) {
				wrv.texture_uv.y = 0;
			}
			// Lightmap coords
			for (int i = 0; i < 2; i++) {
				float u = dot(ti->axis[i], wrv.position - origin);
				u = (u - face.exact_min[i]) / face.exact_span[i];// 0-1 coords on small lightmap image
				//u = (u - 0.5) * 0.90 + 0.5;
				
				u = face.lightmap_min[i]+0.5 + u * (face.lightmap_size[i]-1);	// hack for now to remove pink borders
				wrv.lightmap_uv[i] = u/1200;//256 = lightmap dimensions
			}

			gpu_verts.push_back(wrv);
		}
		// V-2 triangles
		for (int j = 0; j < face.v_count - 2; j++) {
			elements.push_back(first_vert_index);
			elements.push_back(first_vert_index + 1 + j);
			elements.push_back(first_vert_index + (2 + j) % face.v_count);

			num_triangles++;
		}
	}
	// last texture
	RenderMesh rm;
	rm.diffuse = face_textures.at(current_t);
	if (!rm.diffuse->is_loaded()) rm.diffuse = nullptr;
	M_upload_lightmap_mesh(&rm, gpu_verts.data(), elements.data(), gpu_verts.size(), elements.size());
	model->append_mesh(rm);

	// Generate lines for each face
	for (int i = 0; i < wm->faces.size(); i++) {
		const face_t& f = wm->faces.at(i);	// faces have same index

		for (int i = 0; i < f.v_count; i++) {
			line_va.push_2({ wm->verts.at(f.v_start + i), vec3(1.f) }, { wm->verts.at(f.v_start + ((i + 1) % f.v_count)), vec3(1.f) });
		}
	}

	print_info();
}

void WorldGeometry::free_map()
{
	face_textures.clear();
	line_va.clear();
	model->purge_model();
	tree.clear();
}

void WorldGeometry::draw_trace_hits() {
	glDisable(GL_DEPTH_TEST);
	glLineWidth(3);
	hit_faces.draw_array();
	glPointSize(10);
	hit_points.draw_array();
	glLineWidth(1);
	glEnable(GL_DEPTH_TEST);
}
void WorldGeometry::draw_face_edges()
{
	glLineWidth(1);
	glEnable(GL_DEPTH_TEST);
	line_va.draw_array();

}

void WorldGeometry::print_info() const {
	printf("\n******* WORLD GEO ********\n");

	printf("	Faces: %u\n", (int)wm->faces.size());
	printf("	Textures: %u\n", (int)wm->textures.size());
	printf("	Triangles: %u\n", num_triangles);

	printf("**************************\n\n");
}

trace_t WorldGeometry::brute_force_raycast(const ray_t& r)
{
	trace_t res;
	res.start = r.origin;
	res.dir = r.dir;

	const face_t* best = nullptr;
	// Will add a BSP or acceleration structure later
	for (int i = 0; i < wm->faces.size(); i++) {
		face_t& f = wm->faces.at(i);
		float denom = dot(f.plane.normal, r.dir);
		if (abs(denom) < 0.1f) {
			continue;
		}
		float t = -(dot(f.plane.normal, r.origin) + f.plane.d) / denom;
		if (t < 0) {
			continue;
		}
		if (t > r.length) {
			continue;
		}
		// point on the plane
		vec3 point = r.origin + r.dir * t;
		//int v_count = f.v_end - f.v_start;
		bool hit = true;
		for (int i = 0; i < f.v_count; i++) {
			vec3 v =  point -wm->verts.at(f.v_start+i);
			vec3 c = cross(wm->verts.at(f.v_start + (i + 1) % f.v_count)-wm->verts.at(f.v_start + i),v);
			float angle = dot(-f.plane.normal, c);
			if (angle < 0) {
				// point is outside the edges of the polygon
				hit = false;
				break;
			}
		}

		if (hit && (!res.hit||t<res.length)) {
			res.hit = true;
			res.end_pos = point;
			res.length = t;
			res.normal = f.plane.normal;
			res.d = f.plane.d;
			res.face = i;

			best = &f;
		}
	}
	
	if (best) {
		//int count = best->v_end - best->v_start;
		for (int i = 0; i < best->v_count; i++) {
			hit_faces.push_2({ wm->verts.at(best->v_start + i), vec3(1.f,0.0,0.0) }, {wm->verts.at(best->v_start + ((i + 1) % best->v_count)), vec3(1.f,0.0,0.0) });
		}
	}
	

	return res;
}
