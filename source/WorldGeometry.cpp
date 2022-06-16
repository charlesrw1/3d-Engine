#include "WorldGeometry.h"
#include "TextureManager.h"
#include "Texture_def.h"
#include "ModelManager.h"
#include "glad/glad.h"

WorldGeometry global_world;
// disaster code
void WorldGeometry::load_map(const MapParser& mp)
{
	auto faces = mp.get_brush_faces();
	auto texs = mp.get_textures();
	auto verts = mp.get_verts();
	this->world_verts = verts;

	model = new Model;
	line_va = new VertexArray;

	std::vector<RenderVert> gpu_verts;
	std::vector<u32> elements;

	std::vector<ivec2> texture_sizes;
	// first load textures, non-async because width/height is needed for uv coords
	for (const auto& t : texs) {
		face_textures.push_back(global_textures.find_or_load((t + ".png").c_str(), TParams::LOAD_NOW));
		if (face_textures.back()->is_loaded()) {
			texture_sizes.push_back(face_textures.back()->get_dimensions());
		}
		else texture_sizes.push_back(ivec2(256, 256));
	}

	std::vector<u32> indicies(faces.size());
	for (int i = 0; i < indicies.size(); i++) {
		indicies.at(i) = i;
	}
	// sort faces by texture
	std::sort(indicies.begin(), indicies.end(),
		[&faces](const int& i1, const int& i2) -> bool
		{ return faces.at(i1).t_index < faces.at(i2).t_index; });

	int current_t = 0;
	for (int i = 0; i < faces.size(); i++) {
		auto& face = faces.at(indicies.at(i));
		// We have a new texture, append previous mesh
		if (current_t != face.t_index) {
			RenderMesh rm;
			rm.diffuse = face_textures.at(current_t);
			if (!rm.diffuse->is_loaded()) rm.diffuse = nullptr;
			M_upload_mesh(&rm, gpu_verts.data(), elements.data(), gpu_verts.size(), elements.size());

			gpu_verts.clear();
			elements.clear();

			current_t = face.t_index;

			model->append_mesh(rm);
		}
		face_t new_face;
		new_face.v_start = face.v_start;
		new_face.v_end = face.v_end;
		new_face.plane = face.plane;
		
		// Generate verts for GPU

		int v_count = face.v_end - face.v_start;

		int first_vert_index = gpu_verts.size();

		for (int j = 0; j < v_count; j++) {
			RenderVert wrv;
			wrv.normal = -normalize(new_face.plane.normal);
			wrv.position = world_verts.at(new_face.v_start + j);
			
			// Texture coordinates
			wrv.uv.x = dot(wrv.position, face.u_axis) / texture_sizes.at(face.t_index).x * face.uv_scale.x + face.u_offset / texture_sizes.at(face.t_index).x;
			wrv.uv.y = dot(wrv.position, face.v_axis) / texture_sizes.at(face.t_index).y * face.uv_scale.y + face.v_offset / texture_sizes.at(face.t_index).y;
			wrv.uv.x = 0;
			wrv.uv.y = 0;
			// Arbitrary scaling, quake maps are very big
			//wrv.position /= 16.f;

			gpu_verts.push_back(wrv);
		}
		// V-2 triangles
		for(int j=0;j<v_count-2;j++) {
			elements.push_back(first_vert_index);
			elements.push_back(first_vert_index + 1 + j);
			elements.push_back(first_vert_index + (2 + j)%v_count);

			num_triangles++;
		}

		static_faces.push_back(new_face);
	}
	// last texture
	RenderMesh rm;
	rm.diffuse = face_textures.at(current_t);
	if (!rm.diffuse->is_loaded()) rm.diffuse = nullptr;
	M_upload_mesh(&rm, gpu_verts.data(), elements.data(), gpu_verts.size(), elements.size());
	model->append_mesh(rm);

	line_va->set_primitive(VertexArray::Primitive::lines);
	// Generate lines for each face
	for (int i = 0; i < faces.size(); i++) {
		//const poly_t& p = polys.at(i);
		const mface_t& f = faces.at(i);	// faces have same index
		int v_count = f.v_end - f.v_start;

		for (int i = 0; i < v_count; i++) {
			line_va->push_2({ verts.at(f.v_start + i), vec3(1.f) }, { verts.at(f.v_start + ((i + 1) % v_count)), vec3(1.f) });
		}
	}
//	line_va->upload_data();

	print_info();

	hit_faces = new VertexArray;
	hit_faces->set_primitive(VertexArray::Primitive::lines);
	hit_points = new VertexArray;
	hit_points->set_primitive(VertexArray::Primitive::points);

}

void WorldGeometry::debug_draw() {
	//line_va->draw_array();
	hit_faces->draw_array();
	glPointSize(10);
	hit_points->draw_array();
}

void WorldGeometry::print_info() const {
	printf("\n******* WORLD GEO ********\n");

	printf("	Faces: %u\n", (int)static_faces.size());
	printf("	Textures: %u\n", (int)face_textures.size());
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
	for (int i = 0; i < static_faces.size(); i++) {
		face_t& f = static_faces.at(i);
		f.plane.init(world_verts.at(f.v_start), world_verts.at(f.v_start + 1), world_verts.at(f.v_start + 2));
		f.plane.normal = f.plane.normal * -1.f;
		f.plane.d = -dot(f.plane.normal, world_verts.at(f.v_start));

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
		int v_count = f.v_end - f.v_start;
		bool hit = true;
		for (int i = 0; i < v_count; i++) {
			vec3 v =  point -world_verts.at(f.v_start+i);
			vec3 c = cross(world_verts.at(f.v_start + (i + 1) % v_count)-world_verts.at(f.v_start + i),v);
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

	if (best) {
		int count = best->v_end - best->v_start;
		for (int i = 0; i < count; i++) {
			hit_faces->push_2({ world_verts.at(best->v_start + i), vec3(1.f,0.0,0.0) }, { world_verts.at(best->v_start + ((i + 1) % count)), vec3(1.f,0.0,0.0) });
		}
	}

	return res;
}
