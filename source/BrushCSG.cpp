#include "MapParser.h"


#define EQUALS_0(a,b) fabs(a.x-b.x)<0.01&&fabs(a.y-b.y)<0.01&&fabs(a.z-b.z)<0.01

static bool equals(winding_t& a, winding_t& b) {
	if (a.num_verts == b.num_verts) {
		for (int i = 0; i < a.num_verts;i++) {
			for (int j = 0; j < 3; j++) {
				if (abs(a.v[i][j] - b.v[i][j]) > 0.01) {
					return false;
				}
			}
		}
	}
	else {
		return false;
	}
	return true;
}

void MapParser::CSG_union()
{
	// create a copy, these will be modified in the functions
	//clipped_faces = faces;
//	clipped_verts = verts;

	//std::vector<mapface_t> clipped_faces = faces;

	std::vector<mapbrush_t> clipped_brushes = brushes;
	std::vector<mapface_t> clipped_faces;
	//clipped_brushes = brushes;
	//clipped_verts.clear();
	//clipped_faces.clear();

	bool clip_on_plane;
	for (int i = 0; i < brushes.size(); i++) {

		clip_on_plane = false;
		std::vector<mapface_t> final_faces;
		// copy faces
		clipped_brushes.at(i).face_start = 0;
		for (int m = 0; m < brushes.at(i).num_faces; m++) {
			final_faces.push_back(faces.at(brushes.at(i).face_start+m));
			//clipped_brushes.at(i).indices[m] = m;
		}


		for (int j = 0; j < brushes.size(); j++) {

			if (i != j) {
				clip_to_brush(clipped_brushes.at(i), brushes.at(j), clip_on_plane, final_faces);
			}
			else {
				clip_on_plane = true;
			}
		}



		clipped_faces.insert(clipped_faces.end(), final_faces.begin(), final_faces.end());
	}

	brush_model_t bm;
	bm.face_start = face_list.size();
	for (int i = 0; i < clipped_faces.size(); i++)
	{
		face_t f;
		f.plane = clipped_faces[i].plane;
		f.t_info_idx = clipped_faces[i].t_info_idx;
		f.v_start = vertex_list.size();
		winding_t* w = &clipped_faces[i].wind;
		for (int j = 0; j < w->num_verts; j++) {
			vertex_list.push_back(w->v[j]);
		}
		f.v_count = vertex_list.size() - f.v_start;

		face_list.push_back(f);
	}
	bm.face_count = face_list.size() - bm.face_start;
	model_list.push_back(bm);

	// convert map brushes/faces to final models/faces/verticies

	//brushes = std::move(clipped_brushes);
	//verts = std::move(clipped_verts);
	//faces = std::move(clipped_faces);

}
// a is being clipped to b
void MapParser::clip_to_brush(mapbrush_t& to_clip, const mapbrush_t& b, bool clip_to_plane, std::vector<mapface_t>& final_faces)
{
	//printf("Clipping brush...\n");
	//std::vector<mface_t> final_faces;
	//assert(to_clip.num_i < 12);
	std::vector<mapface_t> temp_list;
	temp_list.reserve(final_faces.size());
	for (int i = 0; i < final_faces.size(); i++) {
		auto clipped_brush_faces = clip_to_list(b, 0, final_faces.at(i), clip_to_plane);
		temp_list.insert(temp_list.end(), clipped_brush_faces.begin(), clipped_brush_faces.end());
	}
	//int start = clipped_faces.size();
	//to_clip.num_i = final_faces.size();
	final_faces = temp_list;
	// update indicies

	//for (int i = 0; i < to_clip.num_i ; i++) {
		//to_clip.indices[i] = start + i;
	//}
}
// clips face b to the faces in brush a
std::vector<mapface_t> MapParser::clip_to_list(const mapbrush_t& a, int start_index, const mapface_t& b, bool clip_on_plane)
{
	//printf("	Clipping face...\n");
	std::vector<mapface_t> result;
	assert(start_index < a.num_faces);
	for (int i = start_index; i < a.num_faces; i++) {
		switch (classify_face(faces.at(a.face_start+i), b))
		{
		case FRONT:
			// face is in front of one of a's, due to convexity, its front of all of a's
			result.push_back(b);
			return result;
			break;
		case BACK:
			// no conlusion
			// continue
			break;
		case ON_PLANE: {
			float angle = dot(faces.at(a.face_start+i).plane.normal, b.plane.normal) - 1.f;
			// same normal
			if (angle < 0.1 && angle > -0.1) {
				if (!clip_on_plane) {
					// return polys
					result.push_back(b);
					return result;
				}
			}
			// continue
		}break;
		case SPLIT:
		{
			mapface_t front, back;
			split_face(faces.at(a.face_start+i), b, front, back);

			if (i == a.num_faces - 1) {
				// discard back clipped
				result.push_back(front);
				return result;
			}
			auto check_back = clip_to_list(a, i + 1, back, clip_on_plane);
			if (check_back.empty()) {
				result.push_back(front);
				return result;
			}
			if (equals(check_back.at(0).wind,back.wind)) {
				result.push_back(b);
				return result;
			}
			result.push_back(front);
			result.insert(result.end(), check_back.begin(), check_back.end());
			return result;
		}break;
		}
	}
	return result;
}
// face(param 1) = plane to check other's verticies on
side_t MapParser::classify_face(const mapface_t& face, const mapface_t& other) const
{
	bool front = false, back = false;
//	int other_v_count = other.v_end - other.v_start;
	for (int i = 0; i < other.wind.num_verts; i++) {
		const vec3& v = other.wind.v[i];// verts.at(other.v_start + i);
		float dist = face.plane.dist(v);
		if (dist > 0.1) {

			if (back) {
				return SPLIT;
			}
			front = true;
		}
		else if (dist < -0.1) {
			if (front) {
				return SPLIT;
			}
			back = true;
		}
	}
	if (front) {
		return FRONT;
	}
	else if (back) {
		return BACK;
	}

	return ON_PLANE;
}
// face a being split by plane b
void MapParser::split_face(const mapface_t& b, const mapface_t& a, mapface_t& front, mapface_t& back)
{
	//printf("		Splitting face...\n");
	//int v_count = a.v_end - a.v_start;
	std::vector<side_t> classified(a.wind.num_verts);
	for (int i = 0; i < a.wind.num_verts; i++) {
		classified.at(i) = b.plane.classify_full(a.wind.v[i]);//verts.at(a.v_start + i));
	}

	std::vector<vec3> front_verts;
	std::vector<vec3> back_verts;

	// new faces
	//mface_t front;
	//mface_t back;
	/*
	front.plane = a.plane;
	back.plane = a.plane;
	front.t_index = a.t_index;
	back.t_index = a.t_index;

	
	front.uv_scale = a.uv_scale;
	front.u_axis = a.u_axis;
	front.v_axis = a.v_axis;
	front.v_offset = a.v_offset;
	front.u_offset = a.u_offset;

	back.uv_scale = a.uv_scale;
	back.u_axis = a.u_axis;
	back.v_axis = a.v_axis;
	back.v_offset = a.v_offset;
	back.u_offset = a.u_offset;
	*/

	front = a;
	back = a;
	front.wind.num_verts = 0;
	back.wind.num_verts = 0;


	// Remeber texture info here!

	for (int i = 0; i < a.wind.num_verts; i++) {
		//int idx = a.v_start + i;

		switch (classified.at(i))
		{
		case FRONT:
			front.wind.add_vert(a.wind.v[i]);//verts.at(idx));
			break;
		case BACK:
			back.wind.add_vert(a.wind.v[i]);
			break;
		case ON_PLANE:
			front.wind.add_vert(a.wind.v[i]);
			back.wind.add_vert(a.wind.v[i]);
			break;
		}

		int next_idx = (i + 1)%a.wind.num_verts;
		if (next_idx == a.wind.num_verts) next_idx = 0;
		assert(next_idx != i);

		bool ignore = false;

		if (classified.at(i) == ON_PLANE && classified.at(next_idx) != ON_PLANE) {
			ignore = true;
		}
		if (classified.at(i) != ON_PLANE && classified.at(next_idx) == ON_PLANE) {
			ignore = true;
		}
		if (!ignore && classified.at(i) != classified.at(next_idx)) {
			vec3 vert = vec3(0);
			float percentage;

			if (!b.plane.get_intersection(a.wind.v[i],a.wind.v[next_idx],vert,percentage)) {//verts.at(idx), verts.at(next_idx), vert, percentage)) {

				// should never happen, but it does...
				vert = a.wind.v[i];//verts.at(idx);
				//continue;
				////printf("Split edge not parallel!\n");
				//exit(1);
			}
			front.wind.add_vert(vert);
			back.wind.add_vert(vert);
			//front_verts.push_back(vert);
			//back_verts.push_back(vert);
			// texture calc here


		}
	}
	assert(front.wind.num_verts >= 3);
	assert(back.wind.num_verts >= 3);
	assert(front.wind.num_verts+back.wind.num_verts >= a.wind.num_verts);

	//front.v_start = verts.size();
	//verts.insert(verts.end(), front_verts.begin(), front_verts.end());
	//front.v_end = verts.size();

	//back.v_start = verts.size();
	//verts.insert(verts.end(), back_verts.begin(), back_verts.end());
	//back.v_end = verts.size();
}