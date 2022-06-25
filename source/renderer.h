#ifndef RENDERER_H
#define RENDERER_H

#include "opengl_api.h"

struct rendersettings_t
{
	
};

struct View
{
	int x = 0, y = 0;
	int width = 1600, height = 900;

	bool orthographic = false;
	float fov_y = 45;

	float znear=0.1f, zfar=100.f;
};

class Texture;
class App;
struct SceneData;
class Renderer
{
public:
	Renderer();

	Shader untextured;
	Shader untextured_unshaded;
	Shader textured_mesh;
	Shader point_lights;
	Shader debug_depth;
	Shader depth_render;
	Shader gamma_tm;
	Shader bright_pass_filter;
	Shader guassian_blur;
	Shader upsample_shade;
	Shader no_transform;
	Shader gamma_tm_bloom;
	Shader directional_shadows;
	Shader transformed_primitives;
	Shader leaf;
	Shader model_primitives;
	Shader fresnel;
	Shader overdraw;
	Shader lightmap;

	Texture* white_tex;

	VertexArray quad;

	Framebuffer HDRbuffer;
	Framebuffer new_b;
	Framebuffer intermediate;
	Framebuffer depth_map;

	void render_scene(SceneData& scene);

	void resize(int x, int y) {
		view.x = x;
		view.y = y;
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

	bool enable_bloom;
	float threshold = 2.f;
	float exposure = 1;
	float gamma = 2.2;

	View view;

	bool draw_world = true;

	struct ShadowMapProjection
	{
		float width=27.f;
		float near=0.f, far=80.f;
		float distance=53.f;
	}shadow_map;

	// Matricies for the current frame
	mat4 projection_matrix, view_matrix;

	// Must be called every frame, or they get cleared
	void debug_line(vec3 start, vec3 end, vec3 color);
	void debug_point(vec3 pos, vec3 color);
	void debug_box(vec3 min, vec3 max, vec3 color);

	VertexArray debug_lines;
	VertexArray debug_points;

	VertexArray basic_sphere;

	bool render_lightmap = true;

	bool bloom_debug = false;
	bool show_bright_pass = false;
	int sample_num=0;
	bool down_sample=true;
	bool first_blur_pass=false;


	Texture* lightmap_tex;
private:
	
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