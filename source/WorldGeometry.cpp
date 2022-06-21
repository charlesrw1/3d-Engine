#include "WorldGeometry.h"
#include "TextureManager.h"
#include "Texture_def.h"
#include "ModelManager.h"
#include "glad/glad.h"

WorldGeometry global_world;
// disaster code
void WorldGeometry::load_map(worldmodel_t* worldmodel)
{
	wm = worldmodel;

	model = new Model;
	line_va = new VertexArray;

	hit_faces = new VertexArray;
	hit_faces->set_primitive(VertexArray::Primitive::lines);
	hit_points = new VertexArray;
	hit_points->set_primitive(VertexArray::Primitive::points);

	// KD/bsp tree
	tree.init(wm);
	tree.create_va();
}
void WorldGeometry::create_mesh()
{

	std::vector<LightmapVert> gpu_verts;
	std::vector<u32> elements;

	std::vector<ivec2> texture_sizes;
	// first load textures, non-async because width/height is needed for uv coords
	for (const auto& t : wm->texture_names) {
		face_textures.push_back(global_textures.find_or_load((t + ".png").c_str(), TParams::LOAD_NOW | TParams::GEN_MIPS | TParams::NEAREST));
		if (face_textures.back()->is_loaded()) {
			texture_sizes.push_back(face_textures.back()->get_dimensions());
		}
		else texture_sizes.push_back(ivec2(256, 256));
	}

	std::vector<u32> indicies(wm->faces.size());
	for (int i = 0; i < indicies.size(); i++) {
		indicies.at(i) = i;
	}
	// sort faces by texture
	std::sort(indicies.begin(), indicies.end(),
		[this](const int& i1, const int& i2) -> bool
		{ return wm->t_info.at(wm->faces.at(i1).t_info_idx).t_index < wm->t_info.at(wm->faces.at(i2).t_info_idx).t_index; });

	int current_t = 0;
	for (int i = 0; i < wm->faces.size(); i++) {
		auto& face = wm->faces.at(indicies.at(i));
		// We have a new texture, append previous mesh
		if (current_t != wm->t_info.at(face.t_info_idx).t_index) {
			RenderMesh rm;
			rm.diffuse = face_textures.at(current_t);
			if (!rm.diffuse->is_loaded()) rm.diffuse = nullptr;
			M_upload_lightmap_mesh(&rm, gpu_verts.data(), elements.data(), gpu_verts.size(), elements.size());

			gpu_verts.clear();
			elements.clear();

			current_t = wm->t_info.at(face.t_info_idx).t_index;

			model->append_mesh(rm);
		}
		/*
		face_t new_face;
		new_face.v_start = face.v_start;
		new_face.v_end = face.v_end;
		new_face.plane.init(world_verts.at(face.v_start), world_verts.at(face.v_start + 1), world_verts.at(face.v_start + 2));
		new_face.plane.normal = new_face.plane.normal * -1.f;
		new_face.plane.d = -dot(new_face.plane.normal, world_verts.at(face.v_start));
		new_face.texture_axis[0] = face.u_axis;
		new_face.texture_axis[1] = face.v_axis;
		new_face.texture_scale[0] = face.uv_scale.x;
		new_face.texture_scale[1] = face.uv_scale.y;
		new_face.texture_offset[0] = face.u_offset;
		new_face.texture_offset[1] = face.v_offset;
		*/


		// Generate verts for GPU

		//int v_count = face.. - face.v_start;

		int first_vert_index = gpu_verts.size();

		// Lightmap data
		// recomputed data from lightmapper functions
		const texture_info_t* ti = &wm->t_info.at(face.t_info_idx);
		vec3 tex_normal = normalize(cross(ti->axis[1], ti->axis[0]));
		float ratio = dot(tex_normal, face.plane.normal);
		if (ratio < 0) {
			ratio = -ratio;
			tex_normal = -tex_normal;
		}
		vec3 origin = vec3(0) + normalize(cross(ti->axis[1], ti->axis[0])) * (-face.plane.d / ratio);

		for (int j = 0; j < face.v_count; j++) {
			LightmapVert wrv;
			wrv.normal = normalize(face.plane.normal);
			wrv.position = wm->verts.at(face.v_start + j);

			texture_info_t* ti = &wm->t_info.at(face.t_info_idx);

			// Texture coordinates
			wrv.texture_uv.x = dot(wrv.position * 32.f, ti->axis[0]) / texture_sizes.at(ti->t_index).x / ti->uv_scale.x + ti->offset[0] / texture_sizes.at(ti->t_index).x;
			wrv.texture_uv.y = dot(wrv.position * 32.f, ti->axis[1]) / texture_sizes.at(ti->t_index).y / ti->uv_scale.y + ti->offset[1] / texture_sizes.at(ti->t_index).y;
			if (abs(wrv.texture_uv.x) > 50) {
				wrv.texture_uv.x = 0;
			}
			if (abs(wrv.texture_uv.y) > 50) {
				wrv.texture_uv.y = 0;
			}
			// Lightmap coords
			for (int i = 0; i < 2; i++) {
				float u = dot(ti->axis[i], wrv.position - origin);
				u = (u - face.exact_min[i]) / face.exact_span[i];// 0-1 coords on small lightmap image
				//u = (u - 0.5) * 0.90 + 0.5;
				u = face.lightmap_min[i] + u * face.lightmap_size[i];
				wrv.lightmap_uv[i] = u/800.f;//256 = lightmap dimensions
			}

			gpu_verts.push_back(wrv);
		}
		// V-2 triangles
		for (int j = 0; j < face.v_count - 2; j++) {
			elements.push_back(first_vert_index);
			elements.push_back(first_vert_index + 1 + j);
			elements.push_back(first_vert_index + (2 + j) % face.v_count);

			num_triangles++;
		}

		//static_faces.push_back(new_face);
	}
	// last texture
	RenderMesh rm;
	rm.diffuse = face_textures.at(current_t);
	if (!rm.diffuse->is_loaded()) rm.diffuse = nullptr;
	M_upload_lightmap_mesh(&rm, gpu_verts.data(), elements.data(), gpu_verts.size(), elements.size());
	model->append_mesh(rm);

	line_va->set_primitive(VertexArray::Primitive::lines);
	// Generate lines for each face
	for (int i = 0; i < wm->faces.size(); i++) {
		//const poly_t& p = polys.at(i);
		const face_t& f = wm->faces.at(i);	// faces have same index
		//int v_count = f.v_end - f.v_start;

		for (int i = 0; i < f.v_count; i++) {
			line_va->push_2({ wm->verts.at(f.v_start + i), vec3(1.f) }, { wm->verts.at(f.v_start + ((i + 1) % f.v_count)), vec3(1.f) });
		}
	}
	//	line_va->upload_data();

	print_info();
}

void WorldGeometry::debug_draw() {
	glLineWidth(1);
	glEnable(GL_DEPTH_TEST);
	//line_va->draw_array();
	glDisable(GL_DEPTH_TEST);
	hit_faces->draw_array();
	glPointSize(10);
	hit_points->draw_array();
	glLineWidth(1);
	glEnable(GL_DEPTH_TEST);
	tree.draw();
}

void WorldGeometry::print_info() const {
	printf("\n******* WORLD GEO ********\n");

	printf("	Faces: %u\n", (int)wm->faces.size());
	printf("	Textures: %u\n", (int)wm->textures.size());
	printf("	Triangles: %u\n", num_triangles);

	printf("**************************\n\n");
}

trace_t WorldGeometry::test_ray(const ray_t& r)
{
	trace_t res;
	res.start = r.origin;
	res.dir = r.dir;

	const face_t* best = nullptr;
	// Will add a BSP or acceleration structure later
	for (int i = 0; i < wm->faces.size(); i++) {
		face_t& f = wm->faces.at(i);
		//f.plane.init(world_verts.at(f.v_start), world_verts.at(f.v_start + 1), world_verts.at(f.v_start + 2));
		//f.plane.normal = f.plane.normal * -1.f;
		//f.plane.d = -dot(f.plane.normal, world_verts.at(f.v_start));

		float denom = dot(f.plane.normal, r.dir);
		if (abs(denom) < 0.1f) {
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
		//int v_count = f.v_end - f.v_start;
		bool hit = true;
		for (int i = 0; i < f.v_count; i++) {
			vec3 v =  point -wm->verts.at(f.v_start+i);
			vec3 c = cross(wm->verts.at(f.v_start + (i + 1) % f.v_count)-wm->verts.at(f.v_start + i),v);
			float angle = dot(-f.plane.normal, c);
			if (angle < 0) {
				// point is outside the edges of the polygon
				hit = false;
				break;
			}
		}

		if (hit && (!res.hit||t<res.length)) {
			res.hit = true;
			res.end_pos = point;
			res.length = t;
			res.normal = f.plane.normal;
			res.d = f.plane.d;

			best = &f;
		}
	}

	if (best) {
		//int count = best->v_end - best->v_start;
		for (int i = 0; i < best->v_count; i++) {
			hit_faces->push_2({ wm->verts.at(best->v_start + i), vec3(1.f,0.0,0.0) }, {wm->verts.at(best->v_start + ((i + 1) % best->v_count)), vec3(1.f,0.0,0.0) });
		}
	}

	return res;
}
static u32 total_split = 0;
void KDTree::init(const worldmodel_t* wm)
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
		fx.min = vec3(1024);
		fx.max = vec3(-1024);
		fx.face_index = i;
		for (int j = 0; j < f.v_count; j++) {
			fx.min = glm::min(fx.min, geo->verts.at(f.v_start + j));
			fx.max = glm::max(fx.max, geo->verts.at(f.v_start + j));
		}

		fx.center = (fx.min + fx.max) / 2.f;
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
	subdivide(cut, 0,1);

	flatten_arrays();

	printf("Tree created\n");
	printf("	Num nodes: %d\n", (int)nodes.size());
	printf("	Num leaves: %d\n", (int)leaves.size());
	printf("	Split planes: %u\n", total_split);
}

void KDTree::subdivide(int cut, int node_idx, int depth)
{
	node_t& node = nodes.at(node_idx);
	assert(node.num_faces >= 0);
	if (node.num_faces <= MAX_PER_LEAF || depth > MAX_DEPTH) {
		return;
	}

	plane_t p;
	p.normal = vec3(0);
	p.normal[cut] = 1.f;

	// Find median value
	work_buffer.clear();
	leaf_t* l = &leaves.at(node.first_child);
	while (1) {
	//	work_buffer.push_back(face_extended.at(l->face_index).min[cut]);
		work_buffer.push_back(face_extended.at(l->face_index).min[cut]);
		if (l->next == -1) {
			break;
		}
		l = &leaves.at(l->next);
	}
	std::sort(work_buffer.begin(), work_buffer.end());

	float median = work_buffer.at(work_buffer.size() / 2);
	p.d = -median - 0.1f; //+ 0.5f; // nudge it
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
		bool first = p.classify(fx.max);
		bool second = p.classify(fx.min);

		int next = l->next;
		// face in front
		if (first && second) {
			l->next = child[0].first_child;
			child[0].first_child = current_idx;
			child[0].num_faces++;
		}
		// face behind
		else if (!first && !second) {
			l->next = child[1].first_child;
			child[1].first_child = current_idx;
			child[1].num_faces++;
		}
		// face spans dividing plane
		else {
			total_split++;

			l->next = child[0].first_child;
			child[0].first_child = current_idx;
			// create a second copied leaf
			leaf_t copy;
			copy.face_index = l->face_index;
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

	subdivide((cut+1)%3, start,depth+1);
	subdivide((cut + 1) % 3, start+1,depth+1);
}
void KDTree::create_va_internal(int node_idx, vec3 min, vec3 max)
{	
	const node_t& node = nodes.at(node_idx);
	if (node.num_faces != -1) {
		return;
	}

	const plane_t& plane = planes.at(node.plane_num);
	vec3 corners[4];

	vec3 new_min=min, new_max=max;
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
		va->push_2(VertexP(corners[i],plane.normal*(float)exp(-node.depth/2.f)), VertexP(corners[(i + 1) % 4], plane.normal * (float)exp(-node.depth / 2.f)));
	}

	create_va_internal(node.first_child, new_min, max);	// front
	create_va_internal(node.first_child+1, min, new_max);	// back

}
void KDTree::flatten_arrays()
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
				const leaf_t& l = leaves.at(next);
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

void KDTree::create_va()
{
	va = new VertexArray;
	va->set_primitive(VertexArray::Primitive::lines);

	create_va_internal(0, vec3(-50), vec3(50));
}
int KDTree::find_leaf(vec3 point) const
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
int KDTree::find_leaf(vec3 point, vec3& min_box, vec3& max_box) const
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
void KDTree::print_trace_stats()
{
	printf("Compares: %u\n", total_compares);
	printf("Calls: %u\n", total_func_calls);
	printf("Leafs: %u\n", total_leafs);
	printf("Skipped: %u\n", skipped);
}

trace_t KDTree::test_ray(const ray_t& ray)
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
void KDTree::test_ray_internal(bool uptree, int caller_node, int node_idx, const ray_t& ray, trace_t& trace)
{
	total_func_calls++;
	const node_t& node = nodes.at(node_idx);
	// node has faces attached to it, check them
	if (node.num_faces != -1) {
		check_ray_leaf_node(node, ray, trace);
		if (trace.hit) trace.node = node_idx;
	}
	// node is a parent, determine where to go next
	else {
		const plane_t& p = planes.at(node.plane_num);
		// classify points
		bool start = p.classify(ray.origin);
		//float start_d = p.dist(ray.origin);
		bool end = p.classify(ray.origin+ray.dir*ray.length);
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
				test_ray_internal(false, node_idx, node.first_child+1, ray, trace);
			}
		}
	}
}