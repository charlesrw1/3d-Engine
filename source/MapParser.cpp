#include "MapParser.h"

#include "glm/geometric.hpp"
#include "glm/ext.hpp"


// For vertex array class
#include "opengl_api.h"

#include <iostream>

constexpr float EPSILON = 0.1;

#define EQUALS_0(val) abs(val) < EPSILON

#define NOT_EQUALS(v1,v2) abs(v1-v2) >= EPSILON


bool equals(vec3& v1, vec3& v2) {
	return abs(v1.x - v2.x) < EPSILON && abs(v1.y - v2.y) < EPSILON && abs(v1.z - v2.z) < EPSILON;
}

bool plane_t::operator!=(const plane_t& other) {
	return NOT_EQUALS(d, other.d) && NOT_EQUALS(normal.x, other.normal.x) && NOT_EQUALS(normal.y, other.normal.y) && NOT_EQUALS(normal.z, other.normal.z);
}

static bool intersect_3_planes(const plane_t& p1, const plane_t& p2, const plane_t& p3, vec3& res)
{
	float denom = dot(p1.normal, cross(p2.normal, p3.normal));
	if (EQUALS_0(denom)) {
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
	std::cout << "Faces after CSG: "<< faces.size() << '\n';
	surf_area = get_surface_area();
	std::cout << "Surface area after CSG: " << surf_area << '\n';


	infile.close();
}

void MapParser::compute_intersections(mbrush_t* brush)
{
	
	//std::cout << "new brush: " << (int)brush->num_i << " faces\n";
	for (int i = 0; i < brush->num_faces; i++) {
		const int face_index = brush->face_start + i;
		faces.at(face_index).v_start = verts.size();

		//std::cout << "	new face: " << faces.at(face_and_poly_index).plane.normal.x << ' ' 
			//<< faces.at(face_and_poly_index).plane.normal.y << ' ' <<  faces.at(face_and_poly_index).plane.normal.z << '\n';
		int added_verts = 0;
		int v_start = faces.at(face_index).v_start;
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
						if (equals(verts.at(v_start + i), res)) {
							goto outside_vertex;
						}
					}
					verts.push_back(res);
					added_verts++;

				}
			outside_vertex:
				continue;

			}
		}
		
		faces.at(face_index).v_end = verts.size();
		
		//std::cout << "	verts on face: " << polys.at(face_and_poly_index).v_end - polys.at(face_and_poly_index).v_start << '\n';
	}
}
void MapParser::sort_verticies(mface_t* face)
{
	int v_count = face->v_end - face->v_start;
	//assert(v_count >= 3);
	if (v_count < 3) {
		printf("Not enough verts!\n");
		return;
	}

	//printf("Sorting poly\n");
//	printf("Starting verts:\n");
	//for (int i = face->v_start; i < face->v_end; i++) {
		//printf("	%f %f %f\n", verts.at(i).x, verts.at(i).y, verts.at(i).z);
	//}

	vec3 center = vec3(0);
	vec3 p_norm = face->plane.normal;//normalize(cross(verts.at(poly->v_start+2) - verts.at(poly->v_start + 1), verts.at(poly->v_start) - verts.at(poly->v_start + 1)));
	for (int s = face->v_start; s < face->v_end; s++) {
		center += verts.at(s);
	}
	center /= v_count;

	for (int n = face->v_start; n < face->v_end-2; n++)
	{
		vec3 a = normalize(verts.at(n) - center);
		plane_t p;
		// Constructs a plane that is perpendicular to the face, test wether points are behind
		p.init(verts.at(n), center, center + p_norm);

		int smallest = -1;
		float smallest_angle = -1;

		for (int m = n + 1; m < face->v_end; m++) {
			float dist = dot(p.normal, verts.at(m)) + p.d;
			// Point is in front of the plane
			if (dist > -0.1) {
				vec3 b = normalize(verts.at(m) - center);
				float angle = dot(a, b);
				if (angle > smallest_angle) {
					smallest_angle = angle;
					smallest = m;
				}
			}
		}

		vec3 temp = verts.at(n + 1);
		verts.at(n + 1) = verts.at(smallest);
		verts.at(smallest) = temp;

	}

	// Get normal of resulting verts
	vec3 new_normal = normalize(cross(verts.at(face->v_start + 2) - verts.at(face->v_start + 1), verts.at(face->v_start) - verts.at(face->v_start + 1)));

	if (dot(new_normal, face->plane.normal) < 0) {
		//printf("*reversed winding*\n");

		// Plane is facing opposite way, reverse winding
		for (int i = 0;i< v_count/2; i++) {
			vec3 temp = verts.at(face->v_start + i);
			verts.at(face->v_start + i) = verts.at(face->v_end - 1 - i);
			verts.at(face->v_end - 1 - i) = temp;
		}
	}


	face->plane.normal = -(face->plane.normal);
	face->plane.d = -dot(face->plane.normal, verts.at(face->v_start));

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
			return;
		}
	}

	// TEMPORARY: moves verts along normal in direction
	//make_clipping_hull();

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
	mentity_t* ent = &entities.back();
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
	mbrush_t* b = &brushes.back();
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
	mface_t* f = &faces.back();

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
	f->t_index = get_texture();

	read_token();
	if (parse_char != '[') {
		parse_fail("Expected '['");
		return R_FAIL;
	}
	parse_vec3(f->u_axis);
	read_str(false);
	f->u_offset = std::stoi(parse_buffer);
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
	parse_vec3(f->v_axis);
	read_str(false);
	f->v_offset = std::stoi(parse_buffer);
	read_token();
	if (parse_char != ']') {
		parse_fail("Expected ']'");
		return R_FAIL;
	}
	// throwaway, "angle"
	read_str(false);
	read_str(false);
	f->uv_scale.x = std::stof(parse_buffer);
	read_str(false);
	f->uv_scale.y = std::stof(parse_buffer);

	return R_GOOD;
}
// Have to implement texture axis!!, should just be the grestest abs() axis of the normal
MapParser::Result MapParser::parse_face_quake()
{
	faces.resize(faces.size() + 1);
	mface_t* f = &faces.back();
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
	f->t_index = get_texture();

	read_str(false);
	read_str(false);
	read_str(false);
	read_str(false);
	read_str(false);

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
		//	std::cout << "Token read, " << c << ", line=" << line << '\n';
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
			//	std::cout << "String read, " << parse_buffer << ", line=" << line << '\n';
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
		//	std::cout << "String read, " << parse_buffer << ", line=" << line << '\n';
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

void MapParser::construct_mesh(VertexArray& va, VertexArray& edges)
{
	va.set_primitive(VertexArray::Primitive::triangle);
	edges.set_primitive(VertexArray::Primitive::lines);

	// Scale verts to something reasonable
	// Also change from xyz to xzy
	for (int i = 0; i < verts.size(); i++) {
		verts.at(i) /= 32.f;
		verts.at(i) = vec3(-verts.at(i).x, verts.at(i).z, verts.at(i).y);
	}


	for (int i = 0; i < faces.size(); i++) {
		//const poly_t& p = polys.at(i);
		mface_t& f = faces.at(i);	// faces have same index
		f.u_axis = vec3(-f.u_axis.x, f.u_axis.z, f.u_axis.y);
		f.v_axis = vec3(-f.v_axis.x, f.v_axis.z, f.v_axis.y);

		f.plane.normal = vec3(-f.plane.normal.x, f.plane.normal.z, f.plane.normal.y);

		int v_count = f.v_end - f.v_start;

		// Construct V-2 triangles (ie tri is 1, quad is 2, pent is 3)
		for (int j = 0; j < v_count - 2; j++) {
			va.push_3({ verts.at(f.v_start),max(f.plane.normal,0.f) }, { verts.at(f.v_start + j + 1),max(f.plane.normal,0.f) }, { verts.at(f.v_start + j + 2),max(f.plane.normal,0.f) });
		}

		for (int i = 0; i < v_count; i++) {
			edges.push_2({ verts.at(f.v_start+i), vec3(1.f) }, { verts.at(f.v_start + ((i + 1) % v_count)), vec3(1.f) });
		}

	}


}

uint64 MapParser::get_surface_area()
{
	uint64 area = 0;
	for (int i = 0; i < faces.size(); i++) {
		int v_count = faces.at(i).v_end - faces.at(i).v_start;
		int start = faces.at(i).v_start;
		for (int j = 0; j < v_count; j++) {
			area += abs(length(cross(verts.at(start + j), verts.at(start + (j + 1) % v_count))))/2.f;
		}
	}
	return area;
}
