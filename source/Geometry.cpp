#include "geometry.h"
#include <vector>
void split_winding(const winding_t& a, const plane_t& plane, winding_t& front, winding_t& back)
{
	std::vector<side_t> classified(a.num_verts);
	for (int i = 0; i < a.num_verts; i++) {
		classified.at(i) = plane.classify_full(a.v[i]);//verts.at(a.v_start + i));
	}
	front.num_verts = 0;
	back.num_verts = 0;

	for (int i = 0; i < a.num_verts; i++) {
		//int idx = a.v_start + i;

		switch (classified.at(i))
		{
		case FRONT:
			front.add_vert(a.v[i]);//verts.at(idx));
			break;
		case BACK:
			back.add_vert(a.v[i]);
			break;
		case ON_PLANE:
			front.add_vert(a.v[i]);
			back.add_vert(a.v[i]);
			break;
		}

		int next_idx = (i + 1) % a.num_verts;
		if (next_idx == a.num_verts) next_idx = 0;
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

			if (!plane.get_intersection(a.v[i], a.v[next_idx], vert, percentage)) {//verts.at(idx), verts.at(next_idx), vert, percentage)) {

				// should never happen, but it does...
				vert = a.v[i];//verts.at(idx);
				//continue;
				////printf("Split edge not parallel!\n");
				//exit(1);
			}
			front.add_vert(vert);
			back.add_vert(vert);
			//front_verts.push_back(vert);
			//back_verts.push_back(vert);
			// texture calc here


		}
	}
	assert(front.num_verts >= 3);
	assert(back.num_verts >= 3);
	assert(front.num_verts + back.num_verts >= a.num_verts);

}
void get_extents(const winding_t& w, vec3& min, vec3& max)
{
	min = vec3(5000);
	max = vec3(-5000);
	for (int i = 0; i < w.num_verts; i++) {
		min = glm::min(w.v[i], min);
		max = glm::max(w.v[i], max);
	}
}
side_t classify_face(const winding_t& f1, const plane_t& p)
{
	if (f1.num_verts < 3) {
		printf("Not enough verts to classify face\n");
		return BACK;
	}
	bool front = false, back = false;

	for (int i = 0; i < f1.num_verts; i++) {
		const vec3& v = f1.v[i];
		float dist = p.dist(v);
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