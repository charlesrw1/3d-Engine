#ifndef RENDERER_H
#define RENDERER_H

#include "opengl_api.h"

struct View
{
	int x = 0, y = 0;
	int width = 1600, height = 900;

	float fov_y = 45;

	float znear=0.1f, zfar=200.f;
};
struct Renderstats
{
	int draw_calls;
	int tris;

	float total_ms;
	float bloom_ms;
	float world_ms;
};

// Forward+ structures
// Fragments find Fruxel theyre in
struct FruxelOffset_GPU
{
	int offset;
};
// Fruxel's offsets points to an array of Indicies, terminated with a -1 index
struct VisibleIndex_GPU
{
	int index;
};
struct PointLight_GPU
{
	vec4 pos;
	vec4 color;
	vec4 radius_and_padding;
};

const int FRUXEL_SIZE = 64;


class Texture;
class App;
struct SceneData;
class Renderer
{
public:
	Renderer();

	Shader point_lights;
	Shader debug_depth;
	Shader depth_render;
	Shader gamma_tm;
	Shader bright_pass_filter;
	Shader guassian_blur;
	Shader no_transform;
	Shader gamma_tm_bloom;
	Shader directional_shadows;
	Shader transformed_primitives;
	Shader model_primitives;
	Shader fresnel;
	Shader overdraw;
	Shader lightmap;
	Shader textured_prim_transform;
	Shader upsample_shade;

	Shader forward_plus;

	Texture* white_tex;

	VertexArray quad;
	VertexArray overlay_quad;

	Framebuffer HDRbuffer;
	Framebuffer intermediate;
	Framebuffer depth_map;

	void render_scene(SceneData& scene);

	void resize(int x, int y) {
		view.width = x;
		view.height = y;
		HDRbuffer.resize(x, y);
		intermediate.resize(x, y);
	}
	void increment_fov(int amt) {
		view.fov_y += amt;
	}
	void set_fov(int new_fov) {
		view.fov_y = new_fov;
	}

	// BLOOM
	uint32_t bright_pass;
	uint32_t downsample[2][6];
	uint32_t upsample[6];

	uint32_t bright_pass_fbo;
	uint32_t downsample_fbo[2][6];
	uint32_t upsample_fbo[6];

	bool enable_bloom=true;
	float threshold = 2.f;
	float exposure = 1;
	float gamma = 2.2;

	View view;

	Renderstats stats;

	int stress_test_world_draw_count = 1;

	bool d_world = true;
	bool d_ents = true;
	bool d_world_face_edges = false;
	bool d_lightmap_debug = false;
	bool d_lightmap_patches = false;
	bool d_trace_hits = false;
	bool d_trace_boxes = false;
	bool d_tree_nodes = false;
	bool lightmap_nearest = false;
	bool d_lightmap_overlay = false;
	bool no_textures = false;

	struct ShadowMapProjection
	{
		float width=27.f;
		float near=0.f, far=80.f;
		float distance=53.f;
	}shadow_map;

	// Matricies for the current frame
	mat4 projection_matrix, view_matrix;

	// Must be called every frame, they get cleared
	void debug_line(vec3 start, vec3 end, vec3 color);
	void debug_point(vec3 pos, vec3 color);
	void debug_box(vec3 min, vec3 max, vec3 color);

	VertexArray debug_lines;
	VertexArray debug_points;

	VertexArray basic_sphere;

	bool bloom_debug = false;
	bool show_bright_pass = false;
	int sample_num=0;
	bool down_sample=true;
	bool first_blur_pass=false;

	Texture* lightmap_tex_nearest;
	Texture* lightmap_tex_linear;

	// Clustered forward storage buffers
	uint32_t light_buffer;
	uint32_t visible_buffer;
	uint32_t offset_buffer;

	int fruxel_width;
	int fruxel_height;


private:

	void init_tiled_rendering();

	// Forward+ funcitons
	void push_lights_to_buffer();
	void calc_fruxel_visibility();

	void lightmap_geo();
	void init_basic_sphere();
	void halt_and_render_to_screen(uint32_t texture_id);
	mat4 render_shadow_map(SceneData& scene);
	void init_bloom();
	void bloom_pass();
	void visualize_overdraw(SceneData& scene);
	void scene_pass(SceneData& scene, Shader& shader);
	void draw_world_geo(Shader& shader);
	void upload_point_lights(SceneData& scene);
	void primitive_debug_pass();
	void bounding_sphere_pass(SceneData& scene);
	void load_shaders();
	mat4 get_projection_matrix();
};
#endif