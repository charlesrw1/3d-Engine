#ifndef RENDERER_H
#define RENDERER_H

#include "opengl_api.h"

struct rendersettings_t
{

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

	// Skybox temp stuff
	//Mesh cube;
	Texture* sky_gradient;
	Shader sky_shader;
	void draw_gradient_skybox(SceneData& scene);
	//---------
	Texture* white_tex;

	Texture* temp;
	VertexArray quad;

	Framebuffer HDRbuffer;

	Framebuffer new_b;

	Framebuffer intermediate;

	Framebuffer depth_map;

	void render_scene(SceneData& scene);

	void on_resize();

	// BLOOM STUFF
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

	bool draw_world = true;

	struct ShadowMapProjection
	{
		float width=10.f;
		float near=49.f, far=64.f;
		float distance=52.f;
	}shadow_map;

	// Matricies for the current frame
	mat4 projection_matrix, view_matrix;

	// Temporary Debug Stuff
	void debug_line(vec3 start, vec3 end, vec3 color);
	void debug_point(vec3 pos, vec3 color);
	void debug_box(vec3 min, vec3 max, vec3 color);

	VertexArray debug_lines;
	VertexArray debug_points;

	VertexArray basic_sphere;


	bool bloom_debug = false;
	// TEMP
	bool show_bright_pass = false;
	int sample_num=0;
	bool down_sample=true;
	bool first_blur_pass=false;


private:
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

	void debug_multi_viewport_bloom();
};
#endif