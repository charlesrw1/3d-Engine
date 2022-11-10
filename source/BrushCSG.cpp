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

static bool aabb_intersect(vec3& min1, vec3& max1, vec3& min2, vec3& max2)
{
	for (int i = 0; i < 3; i++) {
		// + 0.5 cause paranoia
		if (min1[i] > max2[i]+0.5f || min2[i] > max1[i]+0.5f)
			return false;
	}
	return true;
}
static float box_size(vec3 min, vec3 max)
{
	return (max.x - min.x) * (max.y - min.y) * (max.z - min.z);
}
void MapParser::CSG_union()
{
	std::vector<mapface_t> clipped_faces;

	bool clip_on_plane;
	for (int j = 0; j < entities.size(); j++) {
		if (entities.at(j).brush_count ==0) {
			continue;
		}
		int start = entities.at(j).brush_start;
		int count = entities.at(j).brush_count;
		for (int i = start; i < start+count; i++) {

			clip_on_plane = false;
			std::vector<mapface_t> final_faces;
			// copy faces
			for (int m = 0; m < brushes.at(i).num_faces; m++) {
				final_faces.push_back(faces.at(brushes.at(i).face_start + m));
			}
			vec3 clip_min = brushes.at(i).min, clip_max = brushes.at(i).max;
			for (int j = start; j < start+count; j++) {
				if (i != j) {
					vec3 other_min = brushes.at(j).min, other_max = brushes.at(j).max;
					if (!aabb_intersect(clip_min, clip_max, other_min, other_max)) {
						continue;
					}
					clip_to_brush(brushes.at(j), clip_on_plane, final_faces);
				}
				else {
					clip_on_plane = true;
				}
			}

			clipped_faces.insert(clipped_faces.end(), final_faces.begin(), final_faces.end());
		}
		brush_model_t bm;
		bm.min = vec3(0);
		bm.max = vec3(0);
		bm.face_start = face_list.size();
		for (int i = 0; i < clipped_faces.size(); i++)
		{
			face_t f;
			f.plane = clipped_faces[i].plane;
			f.t_info_idx = clipped_faces[i].t_info_idx;
			f.v_start = vertex_list.size();
			winding_t* w = &clipped_faces[i].wind;

			if (w->num_verts < 3) {
				printf("Not enough verts in winding, removing face\n");
				continue;
			}
		
			for (int j = 0; j < w->num_verts; j++) {
				vertex_list.push_back(w->v[j]);
			}
			f.v_count = vertex_list.size() - f.v_start;

			face_list.push_back(f);
		}
		bm.face_count = face_list.size() - bm.face_start;
		model_list.push_back(bm);
		clipped_faces.clear();
	}
}
// a is being clipped to b
void MapParser::clip_to_brush(const mapbrush_t& b, bool clip_to_plane, std::vector<mapface_t>& final_faces)
{
	//printf("Clipping brush...\n");
	//std::vector<mface_t> final_faces;
	//assert(to_clip.num_i < 12);
	std::vector<mapface_t> temp_list;
	temp_list.reserve(final_faces.size());
	for (int i = 0; i < final_faces.size(); i++) {
		auto clipped_brush_faces = clip_to_list(b, 0, final_faces.at(i), clip_to_plane);
		if (clipped_brush_faces.size() <= 1)
			temp_list.insert(temp_list.end(), clipped_brush_faces.begin(), clipped_brush_faces.end());
		else
			temp_list.push_back(final_faces.at(i));
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

	front = a;
	back = a;

	split_winding(a.wind, b.plane, front.wind, back.wind);
	
}