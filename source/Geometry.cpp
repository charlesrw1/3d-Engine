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
bool try_split_winding(const winding_t& a, const plane_t& plane, winding_t& front, winding_t& back)
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
		//assert(next_idx != i);

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

	if (front.num_verts < 3 || back.num_verts < 3) {
		return false;
	}
	return true;
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

float winding_t::get_area() const
{
	float area = 0;
	for (int i = 2; i < num_verts; i++) {
		vec3 e1 = v[i-1] - v[0];
		vec3 e2 = v[i] - v[0];
		area += length(cross(e1, e2))*0.5f;
	}
	return area;
}
vec3 winding_t::get_center() const
{
	vec3 center = vec3(0);
	for (int i = 0; i < num_verts; i++) {
		center += v[i];
	}
	center /= num_verts;
	return center;
}
bool to_barycentric(vec3 p1, vec3 p2, vec3 p3, vec3 point, float& u, float& v, float& w)
{
	vec3 e1 = p2 - p1;
	vec3 e2 = p3 - p1;
	vec3 N = cross(e1, e2);
	float area = length(N) / 2.f;

	e1 = p2 - p1;
	e2 = point - p1;
	vec3 c = cross(e1, e2);
	if (dot(c, N) < 0)
		return false;

	e1 = p3 - p2;
	e2 = point - p2;
	c = cross(e1, e2);
	if (dot(c, N) < 0)
		return false;
	u = (length(c) / 2.f) / area;

	e1 = p1 - p3;
	e2 = point - p3;
	c = cross(e1, e2);
	if (dot(c, N) < 0)
		return false;
	v = (length(cross(e1, e2)) / 2.f) / area;

	w = 1 - u - v;

	return true;
}
vec3 closest_point_on_line(const vec3& A, const vec3& B, const vec3& point)
{
	vec3 AB = B - A;
	float t = dot(point - A, AB) / dot(AB, AB);
	return A + glm::min(glm::max(t, 0.f), 1.f)*AB;


	//vec3 v1 = normalize(B - A);
	//vec3 v2 = point - A;
	//float d = dot(v2, v1);
	//return A+ glm::min(glm::max(d, 0.f), 1.f) * v1;

}
vec3 winding_t::closest_point_on_winding(const vec3& point) const
{
	assert(num_verts >= 3);
	plane_t p;
	p.init(v[1], v[0], v[2]);
	vec3 projected_point = point - p.normal * p.dist(point);	// point on plane
	vec3 best_point=vec3(0);
	float best_dist = 10000.f;
	bool outside = false;
	for (int i = 0; i < num_verts; i++) {
		vec3 vec = point - v[i];
		vec3 c = cross(v[(i + 1) % num_verts] - v[i], vec);
		float angle = dot(-p.normal, c);
		if (angle < -0.005f) {
			vec3 point_on_w= closest_point_on_line(v[i], v[(i + 1) % num_verts], point);
			float dist = dot(point - point_on_w, point - point_on_w);
			if (dist < best_dist || !outside) {
				outside = true;
				best_dist = dist;
				best_point = point_on_w;
			}
		}
	}
	return (outside)?best_point:projected_point;
}

bool winding_t::point_inside(const vec3& point) const
{
	plane_t p;
	p.init(v[1], v[0], v[2]);
	for (int i = 0; i < num_verts; i++) {
		vec3 vec = point - v[i];
		vec3 c = cross(v[(i + 1) % num_verts] - v[i], vec);
		float angle = dot(-p.normal, c);
		if (angle < -0.005f) {
			return false;
		}
	}
	return true;
}