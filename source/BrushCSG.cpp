#include "MapParser.h"


void MapParser::CSG_union()
{
	// create a copy, these will be modified in the functions
	//clipped_faces = faces;
//	clipped_verts = verts;


	clipped_brushes = brushes;
	clipped_verts.clear();
	clipped_faces.clear();

	bool clip_on_plane;
	for (int i = 0; i < brushes.size(); i++) {



		clip_on_plane = false;
		std::vector<mface_t> final_faces;
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


	brushes = std::move(clipped_brushes);
	//verts = std::move(clipped_verts);
	faces = std::move(clipped_faces);

}
// a is being clipped to b
void MapParser::clip_to_brush(mbrush_t& to_clip, const mbrush_t& b, bool clip_to_plane, std::vector<mface_t>& final_faces)
{
	//printf("Clipping brush...\n");
	//std::vector<mface_t> final_faces;
	//assert(to_clip.num_i < 12);
	std::vector<mface_t> temp_list;
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
std::vector<mface_t> MapParser::clip_to_list(const mbrush_t& a, int start_index, const mface_t& b, bool clip_on_plane)
{
	//printf("	Clipping face...\n");
	std::vector<mface_t> result;
	assert(start_index < a.num_faces);
	for (int i = start_index; i < a.num_faces; i++) {
		switch (classify_face(faces.at(a.face_start+i), b))
		{
		case FRONT:
			// face is in front of one of a's, due to convexity, its front of all of a's
			result.push_back(copy_face(b));
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
					result.push_back(copy_face(b));
					return result;
				}
			}
			// continue
		}break;
		case SPLIT:
		{
			mface_t front, back;
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
			if (check_back.at(0).v_start == back.v_start) {
				result.push_back(copy_face(b));
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
SideType MapParser::classify_face(const mface_t& face, const mface_t& other) const
{
	bool front = false, back = false;
	int other_v_count = other.v_end - other.v_start;
	for (int i = 0; i < other_v_count; i++) {
		const vec3& v = verts.at(other.v_start + i);
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
void MapParser::split_face(const mface_t& b, const mface_t& a, mface_t& front, mface_t& back)
{
	//printf("		Splitting face...\n");
	int v_count = a.v_end - a.v_start;
	std::vector<SideType> classified(v_count);
	for (int i = 0; i < v_count; i++) {
		classified.at(i) = b.plane.classify_full(verts.at(a.v_start + i));
	}

	std::vector<vec3> front_verts;
	std::vector<vec3> back_verts;

	// new faces
	//mface_t front;
	//mface_t back;
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
	// Remeber texture info here!

	for (int i = 0; i < v_count; i++) {
		int idx = a.v_start + i;

		switch (classified.at(i))
		{
		case FRONT:
			front_verts.push_back(verts.at(idx));
			break;
		case BACK:
			back_verts.push_back(verts.at(idx));
			break;
		case ON_PLANE:
			front_verts.push_back(verts.at(idx));
			back_verts.push_back(verts.at(idx));
			break;
		}

		int next_idx = idx + 1;
		if (next_idx == a.v_end) next_idx = a.v_start;
		assert(next_idx != idx);

		bool ignore = false;

		if (classified.at(idx - a.v_start) == ON_PLANE && classified.at(next_idx - a.v_start) != ON_PLANE) {
			ignore = true;
		}
		if (classified.at(idx - a.v_start) != ON_PLANE && classified.at(next_idx - a.v_start) == ON_PLANE) {
			ignore = true;
		}
		if (!ignore && classified.at(idx - a.v_start) != classified.at(next_idx - a.v_start)) {
			vec3 vert = vec3(0);
			float percentage;

			if (!b.plane.get_intersection(verts.at(idx), verts.at(next_idx), vert, percentage)) {

				// should never happen, but it does...
				vert = verts.at(idx);
				//continue;
				////printf("Split edge not parallel!\n");
				//exit(1);
			}

			front_verts.push_back(vert);
			back_verts.push_back(vert);
			// texture calc here


		}
	}
	assert(front_verts.size() >= 3);
	assert(back_verts.size() >= 3);
	assert(front_verts.size() + back_verts.size() >= v_count);

	front.v_start = verts.size();
	verts.insert(verts.end(), front_verts.begin(), front_verts.end());
	front.v_end = verts.size();

	back.v_start = verts.size();
	verts.insert(verts.end(), back_verts.begin(), back_verts.end());
	back.v_end = verts.size();
}

mface_t MapParser::copy_face(const mface_t& f1)
{
	mface_t res = f1;
	/*
	res.v_start = clipped_verts.size();
	int v_count = f1.v_end - f1.v_start;
	for (int i = 0; i < v_count; i++) {
		clipped_verts.push_back(verts.at(f1.v_start + i));
	}
	res.v_end = clipped_verts.size();
	*/
	return res;
}
