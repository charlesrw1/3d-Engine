#include "MapParser.h"

#include "glm/geometric.hpp"
#include "glm/ext.hpp"

#include "opengl_api.h"

#include <iostream>

constexpr float EPSILON = 0.1;

bool equals(vec3& v1, vec3& v2) {
	return abs(v1.x - v2.x) < EPSILON && abs(v1.y - v2.y) < EPSILON && abs(v1.z - v2.z) < EPSILON;
}

static bool intersect_3_planes(const plane_t& p1, const plane_t& p2, const plane_t& p3, vec3& res)
{
	float denom = dot(p1.normal, cross(p2.normal, p3.normal));
	if (fabs(denom)<EPSILON) {
		return false;
	}

	res = (-p1.d * cross(p2.normal, p3.normal) - p2.d * cross(p3.normal, p1.normal) - p3.d * cross(p1.normal, p2.normal)) / denom;
	return true;
}

void MapParser::start_file(std::string file)
{
	std::cout << "Parsing .map file, " << file << "...\n";
	infile.open(file);
	if (!infile) {
		std::cout << "Couldn't open .map file, " << file << '\n';
		exit(1);
	}
	
	parse_file();
	uint64 surf_area = get_surface_area();
	std::cout << "Finished parse. Entities: " << entities.size() << "; Brushes: " << brushes.size() << "; Faces: " << faces.size() << "; Surface area: " << surf_area << '\n';
	printf("Performing CSG union...\n");
	CSG_union();
	post_process_pass();
	std::cout << "Faces after CSG: "<< faces.size() << '\n';
	surf_area = get_surface_area();
	std::cout << "Surface area after CSG: " << surf_area << '\n';

	infile.close();
}

void MapParser::compute_intersections(mapbrush_t* brush)
{
	
	//std::cout << "new brush: " << (int)brush->num_i << " faces\n";
	for (int i = 0; i < brush->num_faces; i++) {
		const int face_index = brush->face_start + i;
		mapface_t* f = &faces.at(brush->face_start + i);
		//std::cout << "	new face: " << faces.at(face_and_poly_index).plane.normal.x << ' ' 
			//<< faces.at(face_and_poly_index).plane.normal.y << ' ' <<  faces.at(face_and_poly_index).plane.normal.z << '\n';
		int added_verts = 0;
		for (int j = 0; j < brush->num_faces; j++) {
			for (int k = 0; k < brush->num_faces; k++) {
				if (i != j && i != k && j != k) {
					vec3 res = vec3(0);
					bool good = intersect_3_planes(faces.at(brush->face_start+i).plane, faces.at(brush->face_start+j).plane, faces.at(brush->face_start+k).plane, res);
					if (!good) {
						goto outside_vertex;
					}

					for (int m = 0; m < brush->num_faces; m++) {
						const plane_t& p = faces.at(brush->face_start+m).plane;
						// If vertex is outside of all planes
						float dist = dot(p.normal, res) + p.d;
						if (dist < -0.1) {
							goto outside_vertex;	// how evil...
						}
					}
					//std::cout << "		intersection: " << res.x << ' ' << res.y << ' ' << res.z << " : " << i << ' ' << j << ' ' << k << '\n';
					// Good vertex, index to verts is stored in poly[face_index]
					
					// should be moved to end of loop
					for (int i = 0; i < added_verts; i++) {
						if (equals(f->wind.v[i], res)) {
							goto outside_vertex;
						}
					}
					f->wind.add_vert(res);
					added_verts++;

				}
			outside_vertex:
				continue;

			}
		}
		
		//std::cout << "	verts on face: " << polys.at(face_and_poly_index).v_end - polys.at(face_and_poly_index).v_start << '\n';
	}
}
void MapParser::sort_verticies(mapface_t* face)
{
	if (face->wind.num_verts < 3) {
		printf("Not enough verts!\n");
		return;
	}

	//printf("Sorting poly\n");
//	printf("Starting verts:\n");
	//for (int i = face->v_start; i < face->v_end; i++) {
		//printf("	%f %f %f\n", verts.at(i).x, verts.at(i).y, verts.at(i).z);
	//}

	vec3 center = vec3(0);
	vec3 p_norm = face->plane.normal;
	for (int s = 0; s < face->wind.num_verts; s++) {
		center += face->wind.v[s];
	}
	center /= face->wind.num_verts;

	for (int n = 0; n < face->wind.num_verts-2; n++)
	{
		vec3 a = normalize(face->wind.v[n] - center);
		plane_t p;
		// Constructs a plane that is perpendicular to the face, test wether points are behind
		p.init(face->wind.v[n], center, center + p_norm);

		int smallest = -1;
		float smallest_angle = -1;

		for (int m = n + 1; m < face->wind.num_verts; m++) {
			float dist = dot(p.normal, face->wind.v[m]) + p.d;
			// Point is in front of the plane
			if (dist > -0.1) {
				vec3 b = normalize(face->wind.v[m] - center);
				float angle = dot(a, b);
				if (angle > smallest_angle) {
					smallest_angle = angle;
					smallest = m;
				}
			}
		}
		assert(smallest != -1);
		vec3 temp = face->wind.v[n + 1];
		face->wind.v[n + 1] = face->wind.v[smallest];
		face->wind.v[smallest] = temp;

	}

	// Get normal of resulting verts
	vec3 new_normal = normalize(cross(face->wind.v[2] - face->wind.v[1], face->wind.v[0] - face->wind.v[1]));
	if (dot(new_normal, face->plane.normal) < 0) {
		//printf("*reversed winding*\n");

		// Plane is facing opposite way, reverse winding
		for (int i = 0;i< face->wind.num_verts/2; i++) {
			vec3 temp = face->wind.v[i];// verts.at(face->v_start + i);
			face->wind.v[i] = face->wind.v[face->wind.num_verts - 1 - i];// verts.at(face->v_end - 1 - i);
			face->wind.v[face->wind.num_verts - 1 - i] = temp;
		}
	}


	face->plane.normal = -(face->plane.normal);
	face->plane.d = -dot(face->plane.normal, face->wind.v[0]); //verts.at(face->v_start));

	//printf("Ending verts:\n");
	//for (int i = face->v_start; i < face->v_end; i++) {
	//	printf("	%f %f %f\n", verts.at(i).x, verts.at(i).y, verts.at(i).z);
	//}

}

static const vec3 axis_dir[]
{
{0,0,1}	,		// floor
{0,0,-1},		// ceiling
{0,1,0}, 		// south wall
{0,-1,0},		// north wall
{1,0,0}, 			// west wall
{-1,0,0}, 		// east wall
};

// amount to push clip hull
static const float push_amt[]
{
	0,
	64.f,
	16.f,
	16.f,
	16.f,
	16.f,
};


void MapParser::parse_file()
{
	std::stringstream ss;
	ss<< infile.rdbuf();
	file_buffer = ss.str();

	while (true)
	{
		Result r = parse_entity();
		if (r == R_EOF) {
			break;
		}
		else if (r == R_FAIL) {
			std::cout << "Aborting!\n";
			exit(1);
		}
	}
	// Finds verticies that belong to each face
	for (int i = 0; i < brushes.size(); i++) {
		compute_intersections(&brushes.at(i));
	}

	// Sorts verticies in CW winding order
	printf("Sorting verts...\n");
	for (int i = 0; i < brushes.size(); i++) {
		//printf("	Brush num: % d\n", i);

		for (int m = 0; m < brushes.at(i).num_faces; m++) {
			//printf("		Face num:%d\n", m);
			sort_verticies(&faces.at(brushes.at(i).face_start+m));
		}
	}
	// Computes the AABB for the brush
	for (int i = 0; i < brushes.size(); i++) {
		compute_bounds(&brushes.at(i));
	}
}
void MapParser::parse_fail(const char* msg)
{
	std::cout << "Failed map parse @ Line, " << line << ", " << msg << '\n';
}
MapParser::Result MapParser::parse_entity()
{
	Result r = read_token();
	if (r == R_EOF) {
		return R_EOF;
	}
	else if (r == R_FAIL) {
		return R_FAIL;
	}
	if (parse_char != '{') {
		parse_fail("expected '{'");
		return R_FAIL;
	}

	entities.resize(entities.size() + 1);
	entity_t* ent = &entities.back();
	ent->brush_start = brushes.size();
	//u16 b_start = brushes.size();

	while (true)
	{
		Result r = read_token();
		
		switch (parse_char)
		{
		case '"': 
		{
			// Key
			read_str();
			read_token();
			if (parse_char != '"') {
				parse_fail("Expected value after key");
				return R_FAIL;
			}
			std::string key = parse_buffer;
			// Value
			read_str();
			//std::cout << "Property parsed: " << key << ", " << parse_buffer << '\n';
			ent->properties.insert({ key,parse_buffer });
		} break;
		case '{':
			// Start brush list
			if (parse_brush() == R_FAIL) {
				return R_FAIL;
			}


			break;
		case '}':
			// End entity
			ent->brush_count =  brushes.size()-ent->brush_start;
			return R_GOOD;

		default:
			parse_fail("Unknown character");
			return R_FAIL;
		}
	}

	return R_FAIL;
}
MapParser::Result MapParser::parse_brush()
{
	brushes.resize(brushes.size() + 1);
	mapbrush_t* b = &brushes.back();
	b->face_start = faces.size();
	while (true)
	{
		Result r = read_token();
		if (parse_char == '(') {
			if (b->num_faces == MAX_FACES) {
				parse_fail("Too many faces!");
				return R_FAIL;
			}
			
			Result r =  parse_face();
			if (r == R_FAIL) {
				return R_FAIL;
			}

			b->num_faces++;

		}
		else if (parse_char == '}') {
			break;
		}
		else {
			parse_fail("Unknown character on brush parse");
			return R_FAIL;
		}
	}


	return R_GOOD;
}
// parses face for valve's format
MapParser::Result MapParser::parse_face()
{
	faces.resize(faces.size() + 1);
	mapface_t* f = &faces.back();
	texture_info_t ti;


	vec3 verts[3];
	for (int i = 0; i < 3; i++) {
		if (i != 0) {
			read_token();
			if (parse_char != '(') {
				parse_fail("Expected '(' before vec3");
				return R_FAIL;
			}
		}
		Result r = parse_vec3(verts[i]);

		read_token();
		if (parse_char != ')') {
			parse_fail("Expected ')' after vec3");
			return R_FAIL;
		}

	}

	f->plane.init(verts[0], verts[1], verts[2]);

	read_str(false);
	ti.t_index = get_texture();

	read_token();
	if (parse_char != '[') {
		parse_fail("Expected '['");
		return R_FAIL;
	}
	parse_vec3(ti.axis[0]);
	read_str(false);
	ti.offset[0] = std::stoi(parse_buffer);
	read_token();
	if (parse_char != ']') {
		parse_fail("Expected ']'");
		return R_FAIL;
	}

	read_token();
	if (parse_char != '[') {
		parse_fail("Expected '['");
		return R_FAIL;
	}
	parse_vec3(ti.axis[1]);
	read_str(false);
	ti.offset[1] = std::stoi(parse_buffer);
	read_token();
	if (parse_char != ']') {
		parse_fail("Expected ']'");
		return R_FAIL;
	}
	// throwaway, "angle"
	read_str(false);
	read_str(false);
	ti.uv_scale.x = std::stof(parse_buffer);
	read_str(false);
	ti.uv_scale.y = std::stof(parse_buffer);

	t_info.push_back(ti);
	f->t_info_idx = t_info.size() - 1;

	return R_GOOD;
}
static const vec3 baseaxis[18] =
{
{0,0,1}, {1,0,0}, {0,-1,0},			// floor
{0,0,-1}, {1,0,0}, {0,-1,0},		// ceiling
{1,0,0}, {0,1,0}, {0,0,-1},			// west wall
{-1,0,0}, {0,1,0}, {0,0,-1},		// east wall
{0,1,0}, {1,0,0}, {0,0,-1},			// south wall
{0,-1,0}, {1,0,0}, {0,0,-1}			// north wall
};
static void get_texture_axis(const plane_t& plane, vec3& u, vec3& v)
{
	float best = 0;
	int best_axis = 0;
	for (int i = 0; i < 6; i++) {
		float mag = dot(plane.normal, baseaxis[i * 3]);
		if (mag > best) {
			best = mag;
			best_axis = i;
		}
	}

	u = baseaxis[best_axis * 3 + 1];
	v = baseaxis[best_axis * 3 + 2];

}

// Quake .map format: (v1) (v2) (v3) texture x_offset y_offset rotation x_scale y_scale
MapParser::Result MapParser::parse_face_quake()
{
	faces.resize(faces.size() + 1);
	mapface_t* f = &faces.back();
	texture_info_t t_info;
	vec3 verts[3];
	for (int i = 0; i < 3; i++) {
		if (i != 0) {
			read_token();
			if (parse_char != '(') {
				parse_fail("Expected '(' before vec3");
				return R_FAIL;
			}
		}

		Result r = parse_vec3(verts[i]);

		read_token();
		if (parse_char != ')') {
			parse_fail("Expected ')' after vec3");
			return R_FAIL;
		}

	}

	f->plane.init(verts[0], verts[1], verts[2]);

	read_str(false);
	t_info.t_index = get_texture();

	read_str(false);
	t_info.offset[0] = std::stoi(parse_buffer);
	read_str(false);
	t_info.offset[1] = std::stoi(parse_buffer);

	read_str(false);
	float rotation = std::stof(parse_buffer);

	read_str(false);
	t_info.uv_scale[0] = std::stof(parse_buffer);
	read_str(false);
	t_info.uv_scale[1] = std::stof(parse_buffer);
	vec3 uaxis, vaxis;
	get_texture_axis(f->plane, uaxis, vaxis);
	// This needs fixing
	/*
	if (abs(rotation) > 0.01) {

		float cos_ang = cos(radians(rotation));
		float sin_ang = cos(radians(rotation));


		int digu = 0;
		if (abs(uaxis[1]) > 0.5) digu = 1;
		else if (abs(uaxis[2]) > 0.5) digu = 2;
		int digv = 0;
		if (abs(vaxis[1]) > 0.5) digv = 1;
		else if (abs(vaxis[2]) > 0.5) digv = 2;

		float rotu = uaxis[digu] * cos_ang - uaxis[digv] * sin_ang;
		float rotv = uaxis[digu] * sin_ang + uaxis[digv] * cos_ang;
		uaxis[digu] = rotu;
		uaxis[digv] = rotv;

		rotu = vaxis[digu] * cos_ang - vaxis[digv] * sin_ang;
		rotv = vaxis[digu] * sin_ang + vaxis[digv] * cos_ang;
		vaxis[digu] = rotu;
		vaxis[digv] = rotv;
	}
	*/
	t_info.axis[0] = uaxis;
	t_info.axis[1] = vaxis;

	this->t_info.push_back(t_info);
	f->t_info_idx = this->t_info.size() - 1;

	return R_GOOD;
}

MapParser::Result MapParser::parse_vec3(vec3& v)
{
	for (int i = 0; i < 3; i++) {
		read_str(false);

		v[i] = std::stof(parse_buffer);
	}

	return R_GOOD;
}
int MapParser::get_texture()
{
	auto res = str_to_tex_index.find(parse_buffer);
	if (res != str_to_tex_index.end()) {
		return res->second;
	}

	textures.push_back(parse_buffer);
	str_to_tex_index.insert({ parse_buffer,(int)textures.size() - 1 });
	return textures.size() - 1;
}

MapParser::Result MapParser::read_token()
{
	int start = file_ptr;
	while (file_ptr < file_buffer.size())
	{
		char c = file_buffer[file_ptr];

		switch (c)
		{
		case '\n':
			line++;
		case ' ':
		case '\t':
			break;

		case '\0':
			return R_EOF;

		case '/':
			// Consume rest of line
			if (file_buffer.at(file_ptr + 1) == '/') {
				while (file_buffer.at(file_ptr) != '\n' && file_ptr < file_buffer.size()) {
					file_ptr++;
				}
				line++;
			}
			break;
		default:
			parse_char = c;
			file_ptr++;
			return R_GOOD;

		}
		file_ptr++;
	}

	return R_EOF;
}
MapParser::Result MapParser::read_str(bool in_quotes)
{
	int start = file_ptr;
	bool started = false;
	while (file_ptr < file_buffer.size())
	{
		char c = file_buffer[file_ptr];

		switch (c)
		{
		case '\n':
			line++;
		case ' ':
		case '\t':
			if (started && !in_quotes) {
				parse_buffer.assign(file_buffer.begin() + start, file_buffer.begin() + file_ptr);
				file_ptr++;
				return R_GOOD;
			}
			break;

		case '\0':
			return R_EOF;

		case '"':
			if (!in_quotes) 
				break;

			parse_buffer.assign(file_buffer.begin() + start, file_buffer.begin() + file_ptr);
			file_ptr++;
			return R_GOOD;
		default:
			if (!started && !in_quotes) {
				start = file_ptr;
			}
			started = true;
			break;
		}
		file_ptr++;
	}

	return R_EOF;
}
void MapParser::post_process_pass()
{
	int skip_idx = -1;
	for (int i = 0; i < textures.size(); i++) {
		if (textures[i] == "tools/skip") {
			skip_idx = i;
			break;
		}
	}
	
	for (int i = 0; i < vertex_list.size(); i++) {
		auto& v = vertex_list.at(i);
		v /= 32.f;
		v = vec3(-v.x, v.z, v.y);
	}
	for (int i = 0; i < t_info.size(); i++) {
		auto& ti = t_info.at(i);
		for (int j = 0; j < 2; j++) {
			ti.axis[j] = vec3(-ti.axis[j].x, ti.axis[j].z, ti.axis[j].y);
		}
	}

	texture_info_t* ti;
	for (int i = 0; i < face_list.size(); i++) {
		auto& f = face_list.at(i);
		ti = &t_info.at(f.t_info_idx);
		if (ti->t_index == skip_idx) {
			f.dont_draw = true;
		}
		//f.plane.normal = vec3(-f.plane.normal.x, f.plane.normal.z, f.plane.normal.y);
		f.plane.init(vertex_list.at(f.v_start+1), vertex_list.at(f.v_start), vertex_list.at(f.v_start + 2));
		//f.plane.normal *= -1.f;
		//f.plane.d = -dot(f.plane.normal, vertex_list.at(f.v_start));
		if (std::isnan(f.plane.d)) {
			printf("Degenerate plane!\n");
			f.plane.normal = vec3(1, 0, 0);
			f.plane.d = 0;
		}
	}
}
void MapParser::construct_mesh(VertexArray& va, VertexArray& edges)
{
	va.init(VertexArray::Primitive::TRIANGLES);
	edges.init(VertexArray::Primitive::LINES);

	// Scale verts to something reasonable
	// Also change from xyz to xzy
	auto verts = vertex_list;

	

	for (int i = 0; i < face_list.size(); i++) {
		face_t& f = face_list.at(i);	// faces have same index
		

		int v_count = f.v_count;

		// Construct V-2 triangles (ie tri is 1, quad is 2, pent is 3)
		for (int j = 0; j < v_count - 2; j++) {
			va.push_3({ verts.at(f.v_start),max(f.plane.normal,0.f) }, { verts.at(f.v_start + j + 1),max(f.plane.normal,0.f) }, { verts.at(f.v_start + j + 2),max(f.plane.normal,0.f) });
		}

		for (int i = 0; i < v_count; i++) {
			edges.push_2({ verts.at(f.v_start+i), vec3(1.f) }, { verts.at(f.v_start + ((i + 1) % v_count)), vec3(1.f) });
		}

	}


}
void MapParser::compute_bounds(mapbrush_t* mb)
{
	mb->min = vec3(4000);
	mb->max = vec3(-4000);
	for (int i = 0; i < mb->num_faces; i++) {
		const mapface_t* mf = &faces.at(mb->face_start + i);
		for (int j = 0; j < mf->wind.num_verts; j++) {
			mb->min = glm::min(mb->min, mf->wind.v[j]);
			mb->max = glm::max(mb->max, mf->wind.v[j]);
		}
	}
}
uint64 MapParser::get_surface_area()
{
	return 0;
}

