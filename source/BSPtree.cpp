#include"WorldGeometry.h"
#include "BSPtree.h"
#include <algorithm>

static u32 total_split = 0;
void BSPtree::init(const worldmodel_t* wm)
{
	geo = wm;

	nodes.push_back(node_t());
	nodes.back().num_faces = geo->faces.size();
	nodes.back().first_child = 0;

	work_buffer.reserve(geo->faces.size());

	// Get mins/maxes for all faces
	for (int i = 0; i < geo->faces.size(); i++) {
		const face_t& f = geo->faces.at(i);
		face_ex_t fx;
		fx.face_index = i;
		for (int j = 0; j < f.v_count; j++) {
			fx.verts.add_vert(geo->verts.at(f.v_start + j));
		}
		get_extents(fx.verts, fx.min, fx.max);

		face_extended.push_back(fx);
	}
	// add all the starting leaves
	for (int i = 0; i < geo->faces.size(); i++) {
		leaf_t l;
		l.face_index = i;
		l.next = i + 1;
		leaves.push_back(l);
	}
	leaves.back().next = -1;
	int cut = CUT_X;
	subdivide(cut, 0, 1);

	flatten_arrays();

	face_extended = std::vector<face_ex_t>();

	printf("Tree created\n");
	printf("	Num nodes: %d\n", (int)nodes.size());
	printf("	Num leaves: %d\n", (int)leaves.size());
	printf("	Split planes: %u\n", total_split);
}

void BSPtree::subdivide(int cut, int node_idx, int depth)
{
	node_t& node = nodes.at(node_idx);
	assert(node.num_faces >= 0);
	if (node.num_faces <= MAX_PER_LEAF || depth > MAX_DEPTH) {
		return;
	}


	// Find median value
	work_buffer.clear();
	vec3 min(4000), max(-4000);

	leaf_t* l = &leaves.at(node.first_child);
	while (1) {
		min = glm::min(face_extended.at(l->face_index).min, min);
		max = glm::max(face_extended.at(l->face_index).max,max);
		if (l->next == -1) {
			break;
		}
		l = &leaves.at(l->next);
	}
	l = &leaves.at(node.first_child);

	vec3 extents = max - min;
	float biggest = -1;
	int cut_axis = 0;
	for (int i = 0; i < 3; i++) {
		if (extents[i] > biggest) {
			biggest = extents[i];
			cut_axis = i;
		}
	}
	// cut along the greatest axis
	plane_t p;
	p.normal = vec3(0);
	p.normal[cut_axis] = 1.f;

	while (1) {
		//	work_buffer.push_back(face_extended.at(l->face_index).min[cut]);
		work_buffer.push_back(face_extended.at(l->face_index).min[cut_axis]);
		if (l->next == -1) {
			break;
		}
		l = &leaves.at(l->next);
	}
	std::sort(work_buffer.begin(), work_buffer.end());

	float median = work_buffer.at(work_buffer.size() / 2);
	p.d = -median; 
	planes.push_back(p);
	node.plane_num = planes.size() - 1;

	node_t child[2];
	child[0].num_faces = 0;
	child[1].num_faces = 0;
	child[0].depth = depth;
	child[1].depth = depth;
	child[0].parent = node_idx;
	child[1].parent = node_idx;

	// Classify all points
	int current_idx = node.first_child;
	l = &leaves.at(current_idx);
	while (1) {
		const face_ex_t& fx = face_extended.at(l->face_index);
		float first = p.dist(fx.max);
		float second = p.dist(fx.min);

		int next = l->next;
		// face in front
		if (first > -0.005 && second > -0.005) {
			l->next = child[0].first_child;
			child[0].first_child = current_idx;
			child[0].num_faces++;
		}
		// face behind
		else if (first < 0.005 && second < 0.005) {
			l->next = child[1].first_child;
			child[1].first_child = current_idx;
			child[1].num_faces++;
		}
		// face spans dividing plane
		else {
			total_split++;
			winding_t front;
			face_ex_t back_face;

			split_winding(face_extended.at(l->face_index).verts, p,front,back_face.verts);

			l->next = child[0].first_child;
			child[0].first_child = current_idx;
			face_extended.at(l->face_index).verts = front;
			get_extents(front, face_extended.at(l->face_index).min, face_extended.at(l->face_index).max);
			// create a second copied leaf

			back_face.face_index = face_extended.at(l->face_index).face_index;
			get_extents(back_face.verts, back_face.min, back_face.max);
			face_extended.push_back(back_face);

			leaf_t copy;
			copy.face_index = face_extended.size() - 1;
			copy.next = child[1].first_child;
			leaves.push_back(copy);
			child[1].first_child = leaves.size() - 1;

			child[0].num_faces++;
			child[1].num_faces++;
		}

		// next is the linked list for the current subdividing node
		if (next == -1) {
			break;
		}
		current_idx = next;
		l = &leaves.at(current_idx);
	}

	// Finish up
	node.num_faces = -1;
	int start = nodes.size();
	node.first_child = start;
	nodes.push_back(child[0]);
	nodes.push_back(child[1]);

	// Call recurisvely

	//cut = (cut == 0) ? 2 : 0;

	subdivide((cut + 1) % 3, start, depth + 1);
	subdivide((cut + 1) % 3, start + 1, depth + 1);
}
void BSPtree::create_va_internal(int node_idx, vec3 min, vec3 max)
{
	const node_t& node = nodes.at(node_idx);
	if (node.num_faces != -1) {
		return;
	}

	const plane_t& plane = planes.at(node.plane_num);
	vec3 corners[4];

	vec3 new_min = min, new_max = max;
	// trash...
	int dig = 0;
	if (plane.normal.y > 0.5) dig = 1;
	else if (plane.normal.z > 0.5) dig = 2;

	new_min[dig] = -plane.d;
	new_max[dig] = -plane.d;

	corners[0] = new_min;
	corners[1] = new_min;
	corners[2] = new_max;
	corners[3] = new_max;

	corners[1][(dig + 1) % 3] = new_max[(dig + 1) % 3];
	corners[3][(dig + 1) % 3] = new_min[(dig + 1) % 3];

	for (int i = 0; i < 4; i++) {
		va->push_2(VertexP(corners[i], plane.normal * (float)exp(-node.depth / 2.f)), VertexP(corners[(i + 1) % 4], plane.normal * (float)exp(-node.depth / 2.f)));
	}

	create_va_internal(node.first_child, new_min, max);	// front
	create_va_internal(node.first_child + 1, min, new_max);	// back

}
void BSPtree::flatten_arrays()
{
	std::vector<leaf_t> temp_leaves;
	std::vector<int> temp_nodes;

	temp_nodes.push_back(0);

	while (!temp_nodes.empty())
	{
		int index = temp_nodes.back();
		temp_nodes.pop_back();

		node_t& node = nodes.at(index);

		if (node.num_faces != -1) {

			int start_temp = temp_leaves.size();
			int count_temp = 0;
			int next = node.first_child;
			while (next != -1)
			{
				leaf_t& l = leaves.at(next);
				l.face_index = face_extended.at(l.face_index).face_index;	// convert back to world faces
				temp_leaves.push_back(l);
				count_temp++;
				next = l.next;
			}
			node.first_child = start_temp;
			assert(count_temp == node.num_faces);
		}
		else {
			int start = node.first_child;

			temp_nodes.push_back(start);
			temp_nodes.push_back(start + 1);
		}
	}

	leaves = std::move(temp_leaves);

}

void BSPtree::create_va()
{
	va = new VertexArray;
	va->set_primitive(VertexArray::Primitive::lines);

	create_va_internal(0, vec3(-50), vec3(50));
}
int BSPtree::find_leaf(vec3 point) const
{
	int node_n = 0;
	const node_t* node = &nodes.at(node_n);
	while (node->num_faces == -1)
	{
		const plane_t* p = &planes.at(node->plane_num);
		bool back = !p->classify(point);

		node_n = node->first_child + back;
		node = &nodes.at(node_n);
	}
	return node_n;
}
int BSPtree::find_leaf(vec3 point, vec3& min_box, vec3& max_box) const
{
	min_box = vec3(-64);
	max_box = vec3(64);

	int node_n = 0;
	const node_t* node = &nodes.at(node_n);
	while (node->num_faces == -1)
	{
		const plane_t* p = &planes.at(node->plane_num);
		bool front = p->classify(point);

		int dig = 0;
		if (p->normal.y > 0.5) dig = 1;
		else if (p->normal.z > 0.5) dig = 2;

		if (front) {
			min_box[dig] = -p->d;
			node_n = node->first_child;
		}
		else {
			max_box[dig] = -p->d;
			node_n = node->first_child + 1;
		}
		node = &nodes.at(node_n);
	}
	return node_n;

	//return binary_search(0, point, min_box, max_box);
}
static u32 total_compares = 0;
static u32 total_func_calls = 0;
static u32 total_leafs = 0;
static u32 skipped = 0;
void BSPtree::print_trace_stats()
{
	printf("Compares: %u\n", total_compares);
	printf("Calls: %u\n", total_func_calls);
	printf("Leafs: %u\n", total_leafs);
	printf("Skipped: %u\n", skipped);
}

trace_t BSPtree::test_ray(const ray_t& ray)
{
	total_compares = 0;
	total_func_calls = 0;
	total_leafs = 0;
	skipped = 0;
	//curr_compare++;
	int start_node = find_leaf(ray.origin);
	trace_t t;
	t.start = ray.origin;
	t.dir = ray.dir;
	// first test start node alone
	check_ray_leaf_node(nodes.at(start_node), ray, t);
	if (!t.hit) {
		test_ray_internal(true, start_node, nodes.at(start_node).parent, ray, t);
	}

	return t;
}
void BSPtree::test_ray_internal(bool uptree, int caller_node, int node_idx, const ray_t& ray, trace_t& trace)
{
	total_func_calls++;
	const node_t& node = nodes.at(node_idx);
	// node has faces attached to it, check them
	if (node.num_faces != -1) {
		int face = trace.face;
		check_ray_leaf_node(node, ray, trace);
		if (trace.hit && face != trace.face) 
			trace.node = node_idx;
	}
	// node is a parent, determine where to go next
	else {
		const plane_t& p = planes.at(node.plane_num);
		// classify points
		bool start = p.classify(ray.origin);
		//float start_d = p.dist(ray.origin);
		bool end = p.classify(ray.origin + ray.dir * ray.length);
		//float end_d = p.dist(ray.origin);

		// caller came from below, check other half of tree
		if (uptree) {
			if (trace.hit) {
				return;
			}

			assert(caller_node == node.first_child || caller_node == node.first_child + 1);
			// caller node was front face
			bool caller_front = caller_node == node.first_child;

			if (!caller_front && (start || end)) {
				test_ray_internal(false, node_idx, node.first_child, ray, trace);
			}
			else if (caller_front && (!start || !end)) {
				test_ray_internal(false, node_idx, node.first_child + 1, ray, trace);
			}
			if (node_idx != 0) {
				test_ray_internal(true, node_idx, node.parent, ray, trace);
			}
		}
		// we are going down the tree, either leaf is a potential canidate
		else {
			if (start || end) {
				test_ray_internal(false, node_idx, node.first_child, ray, trace);
			}
			if (!start || !end) {
				test_ray_internal(false, node_idx, node.first_child + 1, ray, trace);
			}
		}
	}
}