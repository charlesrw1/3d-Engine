#include "WorldGeometry.h"

struct tracestack_t
{
	vec3 backpt;
	int node;	// index into node array
	bool side;	// true = front
};

struct tracestack_debug_t
{
	vec3 backpt;
	int node;
	bool side;

	vec3 box_min;
	vec3 box_max;

	vec3 last_cut;

	int depth;
};

trace_t BSPtree::test_ray(vec3 start, vec3 end, float epsilon)
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
			r.origin = front;
			r.length = length(back - front)+0.01f;
			check_ray_leaf_node(*node, r, trace, epsilon);
			if (trace.hit) {
//va->push_2({ start,vec3(1.f,0.0,0.0) }, { end,vec3(1.f,0.0,0.0) });
				trace.node = node_n;
				return trace;
			}

			// no more nodes to check, no hit
			top--;
			if (top < stack) {
//a->push_2({ start,vec3(0,1.f,0) }, { end,vec3(0,1.f,0) });
				return trace;
			}

			front = back;//-(r.dir*0.05f);
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
		bool front_side = front_dist < 0.01;
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
#include <iostream>
trace_t BSPtree::test_ray_debug(vec3 start, vec3 end)
{
	trace_t trace;
	trace.hit = false;
	trace.dir = normalize(end - start);
	trace.start = start;
	ray_t r;
	r.dir = trace.dir;
	r.origin = start;
	r.length = length(end - start);
	int node_n;
	vec3 front;
	vec3 back;
	tracestack_debug_t stack[64];
	tracestack_debug_t* top;
	const node_t* node;
	node_n = 0;
	top = stack;
	front = start;
	back = end;
	vec3 max_box = vec3(32);
	vec3 min_box = vec3(-32);
	vec3 last_cut = vec3(1, 1, 1);
	box_array.clear();
	va.clear();
	int num_poly_checks = 0;
	while (1)
	{
		node = &nodes[node_n];
		while (node->num_faces != -1) {
			std::cout << "Checking node " << node_n <<  " depth: " << node->depth << " faces: " << node->num_faces << " (parent " << node->parent << ")\n";
			r.origin = front;
			r.length = length(back - front)+0.01f;
			box_array.add_solid_box(min_box, max_box, vec4(last_cut, node->num_faces/80.f));
			num_poly_checks += node->num_faces;
			check_ray_leaf_node(*node, r, trace, -0.005f);
			if (trace.hit) {
				std::cout << "Num compares: " << num_poly_checks << '\n';
				va.push_2({ start,vec3(1.f,0.0,0.0) }, { end,vec3(1.f,0.0,0.0) });
				trace.node = node_n;
				return trace;
			}

			// no more nodes to check, no hit
			top--;
			if (top < stack) {
				std::cout << "Num compares: " << num_poly_checks << '\n';
				va.push_2({ start,vec3(0,1.f,0) }, { end,vec3(0,1.f,0) });
				return trace;
			}

			front = back;
			back = top->backpt;
			node_n = top->node;
			max_box = top->box_max;
			min_box = top->box_min;
			last_cut = top->last_cut;

			//node_n = node->first_child + top->side;
			node = &nodes[node_n];
			int temp = node->first_child + top->side;
			node = &nodes[node->first_child + top->side];
			node_n = temp;
		}

		const plane_t* p = &planes[node->plane_num];
		last_cut = abs(p->normal);
		int dig = 0;
		if (p->normal.y > 0.5) dig = 1;
		else if (p->normal.z > 0.5) dig = 2;

		float front_dist = p->distance(front);
		float back_dist = p->distance(back);

		// both in front
		if (front_dist > -0.01 && back_dist > -0.01) {
			min_box[dig] = -p->d;
			node_n = node->first_child;
			continue;
		}
		// both behind
		if (front_dist < 0.01 && back_dist < 0.01) {
			max_box[dig] = -p->d;
			node_n = node->first_child + 1;
			continue;
		}
		bool front_side = front_dist < 0;
		// split, cache mid to back, set next to front to mid
		top->backpt = back;
		top->node = node_n;
		top->side = !front_side;
		top->box_max = max_box;
		top->box_min = min_box;
		top->last_cut = last_cut;
		if (front_side) {
			top->box_min[dig] = -p->d;
			max_box[dig] = -p->d;
		}
		else {
			top->box_max[dig] = -p->d;
			min_box[dig] = -p->d;
		}

		top++;

		front_dist = front_dist / (front_dist - back_dist);
		back = front + front_dist * (back - front);

		node_n = node->first_child + front_side;
	}
	return trace;
}

#define ISLEAF(a) (a&0x80000000)
#define AXIS(a) (a&3)
#define INSIDE_OFFSET(a) ((a&0x7FFFFFFF)>>2)
#define LEAF_OFFSET(a) (a&0x7FFFFFFF)


trace_t BSPtree::test_ray_fast(vec3 start, vec3 end, float epsilon, bool dif_first_epsilon, float first_epsilon)
{
	trace_t trace;
	trace.dir = normalize(end - start);
	trace.start = start;
	ray_t r;
	r.dir = trace.dir;
	r.origin = start;
	r.length = length(end - start);
	int node_n;
	vec3 front;
	vec3 back;
	tracestack_t stack[64];
	tracestack_t* top;
	const BSPNode* node;
	node_n = 0;
	top = stack;
	front = start;
	back = end;
	bool seen_first = false;

	while (1)
	{
		node = &fast_list[node_n];
		while (ISLEAF(node->data1)) {
			r.origin = front;
			r.length = length(back - front) + 0.01f;
			check_ray_bsp_node(*node, r, trace, (!seen_first)?first_epsilon:epsilon);
			seen_first = true;
			if (trace.hit) {
				//va->push_2({ start,vec3(1.f,0.0,0.0) }, { end,vec3(1.f,0.0,0.0) });
				trace.node = node_n;
				return trace;
			}

			// no more nodes to check, no hit
			top--;
			if (top < stack) {
				//a->push_2({ start,vec3(0,1.f,0) }, { end,vec3(0,1.f,0) });
				return trace;
			}

			front = back;//-(r.dir*0.05f);
			back = top->backpt;
			node_n = top->node;

			//node_n = node->first_child + top->side;
			node = &fast_list[node_n];
			int temp = INSIDE_OFFSET(node->data1) + top->side;
			node = &fast_list[temp];
			node_n = temp;
		}

		//const plane_t* p = &planes[node->plane_num];
		int axis = AXIS(node->data1);
		float dist = *(float*)&node->data2;
		float front_dist = front[axis] + dist;// p->distance(front);
		float back_dist = back[axis] + dist;// p->distance(back);

		// both in front
		if (front_dist > -0.01 && back_dist > -0.01) {
			node_n = INSIDE_OFFSET(node->data1);//node->first_child;
			continue;
		}
		// both behind
		if (front_dist < 0.01 && back_dist < 0.01) {
			node_n = INSIDE_OFFSET(node->data1) + 1;
			continue;
		}
		bool front_side = front_dist < 0.01;
		// split, cache mid to back, set next to front to mid
		top->backpt = back;
		top->node = node_n;
		top->side = !front_side;

		top++;

		front_dist = front_dist / (front_dist - back_dist);
		back = front + front_dist * (back - front);

		node_n = INSIDE_OFFSET(node->data1) + front_side;
	}


}

/*
inline void BSPtree::check_ray_leaf_node(const node_t& node, vec3& start, vec3& end, trace_t& trace)
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
			trace.face = leaf->face_index;
		}
	}
}
*/

void BSPtree::check_ray_leaf_node(const node_t& node, const ray_t& r, trace_t& trace, float epsilon)
{
	assert(node.num_faces > -1);
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

		if (t < epsilon) {
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
			if (angle < -0.005f) {
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
			trace.face = leaf->face_index;
		}
	}
}
void BSPtree::check_ray_bsp_node(const BSPNode& node, const ray_t& r, trace_t& trace, float epsilon)
{
	int count = node.data2;
	int offset = LEAF_OFFSET(node.data1);
	assert(count >= 0);

	for (int i = offset; i <offset + count; i++) {
		const face_t& f = geo->faces.at(face_index.at(i));

		float denom = dot(f.plane.normal, r.dir);

		if (abs(denom) < 0.01f) {
			continue;
		}
		float t = -(dot(f.plane.normal, r.origin) + f.plane.d) / denom;

		if (t < epsilon) {
			continue;
		}
		if (t > r.length) {
			continue;
		}
		// point on the plane
		vec3 point = r.origin + r.dir * t;
		int v_count = f.v_count;
		bool hit = true;
		for (int i = 0; i < v_count; i++) {
			vec3 v = point - geo->verts[f.v_start + i];
			vec3 c = cross(geo->verts[f.v_start + (i + 1) % v_count]- geo->verts[f.v_start + i], v);
			float angle = dot(-f.plane.normal, c);
			if (angle < -0.005f) {
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
			//trace.face = i;//leaf->face_index;
		}
	}
}