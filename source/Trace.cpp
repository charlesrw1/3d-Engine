#include "Trace.h"

struct tracestack_t
{
	vec3 backpt;
	int node;	// index into node array
	bool side;	// true = front
};

trace_t KDTree::test_ray(vec3 start, vec3 end)
{
	trace_t trace;
	trace.dir = normalize(end - start);
	trace.start = start;
	ray_t r;
	r.dir = trace.dir;
	r.origin = start;
	r.length = length(end-start);
	int node_n;
	vec3 front;
	vec3 back;
	tracestack_t stack[64];
	tracestack_t* top;
	const node_t* node;
	node_n = 0;
	top = stack;
	front = start;
	back = end;

	while (1)
	{
		node = &nodes[node_n];

		while (node->num_faces != -1) {
			check_ray_leaf_node(*node, r, trace);
			if (trace.hit) {
//va->push_2({ start,vec3(1.f,0.0,0.0) }, { end,vec3(1.f,0.0,0.0) });
				trace.node = node_n;
				return trace;
			}

			// no more nodes to check, no hit
			top--;
			if (top < stack) {
//va->push_2({ start,vec3(0,1.f,0) }, { end,vec3(0,1.f,0) });
				return trace;
			}

			front = back;
			back = top->backpt;
			node_n = top->node;

			//node_n = node->first_child + top->side;
			node = &nodes[node_n];
			int temp = node->first_child + top->side;
			node = &nodes[node->first_child + top->side];
			node_n = temp;
		}

		const plane_t* p = &planes[node->plane_num];
		float front_dist =p->distance(front);
		float back_dist = p->distance(back);

		// both in front
		if (front_dist > -0.01 && back_dist > -0.01) {
			node_n = node->first_child;
			continue;
		}
		// both behind
		if (front_dist < 0.01 && back_dist < 0.01) {
			node_n = node->first_child + 1;
			continue;
		}
		bool front_side = front_dist < 0;
		// split, cache mid to back, set next to front to mid
		top->backpt = back;
		top->node = node_n;
		top->side = !front_side;

		top++;

		front_dist = front_dist / (front_dist - back_dist);
		back = front + front_dist * (back - front);
		
		node_n = node->first_child + front_side;
	}


	return trace_t();
}
inline void KDTree::check_ray_leaf_node(const node_t& node, vec3& start, vec3& end, trace_t& trace)
{
	const leaf_t* leaf;
	vec3 dir = normalize(end - start);
	int count = node.num_faces;
	for (int i = node.first_child; i < node.first_child + count; i++) {
		leaf = &leaves[i];
		const face_t& f = geo->faces.at(leaf->face_index);

		double denom = dot(f.plane.normal, dir);
		// backface or parallel
		if (abs(denom) < 0.00001f) {
			//printf("PARALELL\n");
			continue;
		}
		//if (abs(denom) < 0.1f) {
		//	continue;
		//}
		float t = -(dot(f.plane.normal, start) + f.plane.d) / denom;

		if (t <= 0) {
			continue;
		}
		float length = glm::length(end - start);
		if (t > length) {
			continue;
		}
		// point on the plane
		vec3 point = start + dir * t;
		int v_count = f.v_count;

		bool hit = true;
		for (int i = 0; i < v_count; i++) {
			vec3 v = point - geo->verts[f.v_start + i];
			vec3 c = cross(geo->verts[f.v_start + (i + 1) % v_count] - geo->verts[f.v_start + i], v);
			float angle = dot(-f.plane.normal, c);
			if (angle < 0) {
				// point is outside the edges of the polygon
				hit = false;
				break;
			}
		}

		if (hit && (!trace.hit || t < trace.length)) {
			trace.hit = true;
			trace.end_pos = point;
			trace.length = t;
			trace.normal = f.plane.normal;
			trace.d = f.plane.d;
		}
	}
}
void KDTree::check_ray_leaf_node(const node_t& node, const ray_t& r, trace_t& trace)
{
	assert(node.num_faces > 0);
	const leaf_t* leaf;

	int count = node.num_faces;
	for (int i = node.first_child; i < node.first_child + count; i++) {
		leaf = &leaves.at(i);
		const face_t& f = geo->faces.at(leaf->face_index);


		float denom = dot(f.plane.normal, r.dir);
		// backface
		//if (denom > -0.1f) {
		//	continue;
		//}
		if (abs(denom) < 0.01f) {
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
		int v_count = f.v_count;// f.v_end - f.v_start;
		bool hit = true;
		for (int i = 0; i < v_count; i++) {
			vec3 v = point - geo->verts.at(f.v_start + i);
			vec3 c = cross(geo->verts.at(f.v_start + (i + 1) % v_count) - geo->verts.at(f.v_start + i), v);
			float angle = dot(-f.plane.normal, c);
			if (angle < 0) {
				// point is outside the edges of the polygon
				hit = false;
				break;
			}
		}

		if (hit && (!trace.hit || t < trace.length)) {
			trace.hit = true;
			trace.end_pos = point;
			trace.length = t;
			trace.normal = f.plane.normal;
			trace.d = f.plane.d;
		}
	}
}