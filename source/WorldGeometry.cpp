#include "WorldGeometry.h"
#include "TextureManager.h"
#include "Texture_def.h"
#include "ModelManager.h"
#include "glad/glad.h"

WorldGeometry global_world;
// disaster code
void WorldGeometry::load_map(worldmodel_t* worldmodel)
{
	wm = worldmodel;

	model = new Model;
	line_va = new VertexArray;

	hit_faces = new VertexArray;
	hit_faces->set_primitive(VertexArray::Primitive::lines);
	hit_points = new VertexArray;
	hit_points->set_primitive(VertexArray::Primitive::points);

	// KD/bsp tree
	tree.init(wm);
	tree.create_va();
}
void WorldGeometry::create_mesh()
{

	std::vector<LightmapVert> gpu_verts;
	std::vector<u32> elements;

	std::vector<ivec2> texture_sizes;
	// first load textures, non-async because width/height is needed for uv coords
	for (const auto& t : wm->texture_names) {
		face_textures.push_back(global_textures.find_or_load((t + ".png").c_str(), TParams::LOAD_NOW | TParams::GEN_MIPS | TParams::NEAREST));
		if (face_textures.back()->is_loaded()) {
			texture_sizes.push_back(face_textures.back()->get_dimensions());
		}
		else texture_sizes.push_back(ivec2(256, 256));
	}

	std::vector<u32> indicies(wm->faces.size());
	for (int i = 0; i < indicies.size(); i++) {
		indicies.at(i) = i;
	}
	// sort faces by texture
	std::sort(indicies.begin(), indicies.end(),
		[this](const int& i1, const int& i2) -> bool
		{ return wm->t_info.at(wm->faces.at(i1).t_info_idx).t_index < wm->t_info.at(wm->faces.at(i2).t_info_idx).t_index; });

	int current_t = 0;
	for (int i = 0; i < wm->faces.size(); i++) {
		auto& face = wm->faces.at(indicies.at(i));
		// We have a new texture, append previous mesh
		if (current_t != wm->t_info.at(face.t_info_idx).t_index) {
			RenderMesh rm;
			rm.diffuse = face_textures.at(current_t);
			if (!rm.diffuse->is_loaded()) rm.diffuse = nullptr;
			M_upload_lightmap_mesh(&rm, gpu_verts.data(), elements.data(), gpu_verts.size(), elements.size());

			gpu_verts.clear();
			elements.clear();

			current_t = wm->t_info.at(face.t_info_idx).t_index;

			model->append_mesh(rm);
		}
		/*
		face_t new_face;
		new_face.v_start = face.v_start;
		new_face.v_end = face.v_end;
		new_face.plane.init(world_verts.at(face.v_start), world_verts.at(face.v_start + 1), world_verts.at(face.v_start + 2));
		new_face.plane.normal = new_face.plane.normal * -1.f;
		new_face.plane.d = -dot(new_face.plane.normal, world_verts.at(face.v_start));
		new_face.texture_axis[0] = face.u_axis;
		new_face.texture_axis[1] = face.v_axis;
		new_face.texture_scale[0] = face.uv_scale.x;
		new_face.texture_scale[1] = face.uv_scale.y;
		new_face.texture_offset[0] = face.u_offset;
		new_face.texture_offset[1] = face.v_offset;
		*/


		// Generate verts for GPU

		//int v_count = face.. - face.v_start;

		int first_vert_index = gpu_verts.size();

		// Lightmap data
		// recomputed data from lightmapper functions
		const texture_info_t* ti = &wm->t_info.at(face.t_info_idx);
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
			wrv.texture_uv.x = dot(wrv.position * 32.f, ti->axis[0]) / texture_sizes.at(ti->t_index).x / ti->uv_scale.x + ti->offset[0] / texture_sizes.at(ti->t_index).x;
			wrv.texture_uv.y = dot(wrv.position * 32.f, ti->axis[1]) / texture_sizes.at(ti->t_index).y / ti->uv_scale.y + ti->offset[1] / texture_sizes.at(ti->t_index).y;
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
				wrv.lightmap_uv[i] = u/1500;//256 = lightmap dimensions
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

		//static_faces.push_back(new_face);
	}
	// last texture
	RenderMesh rm;
	rm.diffuse = face_textures.at(current_t);
	if (!rm.diffuse->is_loaded()) rm.diffuse = nullptr;
	M_upload_lightmap_mesh(&rm, gpu_verts.data(), elements.data(), gpu_verts.size(), elements.size());
	model->append_mesh(rm);

	line_va->set_primitive(VertexArray::Primitive::lines);
	// Generate lines for each face
	for (int i = 0; i < wm->faces.size(); i++) {
		//const poly_t& p = polys.at(i);
		const face_t& f = wm->faces.at(i);	// faces have same index
		//int v_count = f.v_end - f.v_start;

		for (int i = 0; i < f.v_count; i++) {
			line_va->push_2({ wm->verts.at(f.v_start + i), vec3(1.f) }, { wm->verts.at(f.v_start + ((i + 1) % f.v_count)), vec3(1.f) });
		}
	}
	//	line_va->upload_data();

	print_info();
}

void WorldGeometry::debug_draw() {
	glLineWidth(1);
	glEnable(GL_DEPTH_TEST);
	//line_va->draw_array();
	glDisable(GL_DEPTH_TEST);
	hit_faces->draw_array();
	glPointSize(10);
	hit_points->draw_array();
	glLineWidth(1);
	glEnable(GL_DEPTH_TEST);
	tree.draw();
}

void WorldGeometry::print_info() const {
	printf("\n******* WORLD GEO ********\n");

	printf("	Faces: %u\n", (int)wm->faces.size());
	printf("	Textures: %u\n", (int)wm->textures.size());
	printf("	Triangles: %u\n", num_triangles);

	printf("**************************\n\n");
}

trace_t WorldGeometry::test_ray(const ray_t& r)
{
	trace_t res;
	res.start = r.origin;
	res.dir = r.dir;

	const face_t* best = nullptr;
	// Will add a BSP or acceleration structure later
	for (int i = 0; i < wm->faces.size(); i++) {
		face_t& f = wm->faces.at(i);
		//f.plane.init(world_verts.at(f.v_start), world_verts.at(f.v_start + 1), world_verts.at(f.v_start + 2));
		//f.plane.normal = f.plane.normal * -1.f;
		//f.plane.d = -dot(f.plane.normal, world_verts.at(f.v_start));

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

			best = &f;
		}
	}
	/*
	if (best) {
		//int count = best->v_end - best->v_start;
		for (int i = 0; i < best->v_count; i++) {
			hit_faces->push_2({ wm->verts.at(best->v_start + i), vec3(1.f,0.0,0.0) }, {wm->verts.at(best->v_start + ((i + 1) % best->v_count)), vec3(1.f,0.0,0.0) });
		}
	}
	*/

	return res;
}
