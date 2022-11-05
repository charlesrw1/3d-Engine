#include "renderer.h"
#include "app.h"
#include "types.h"
#include "camera.h"
#include "glad/glad.h"

#include "Texture_def.h"
#include "TextureManager.h"

#include "WorldGeometry.h"
#include "Light.h"
#include <iostream>
#include <chrono>

const int SHADOW_WIDTH = 2048;

class Stopwatch
{
public:
	using Clock =std::chrono::high_resolution_clock;
	using Time = std::chrono::steady_clock::time_point;
	void start() {
		start_time = Clock::now();
	}
	float end() {
		auto m = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start_time);
		return m.count()/1000.f;
	}
private:
		Time start_time;
};

void gen_cubemap_views();
Renderer::Renderer()
{
	load_shaders();
	debug_lines.init(VAPrim::LINES);
	debug_points.init(VAPrim::POINTS);

	quad.init(VAPrim::TRIANGLES);
	overlay_quad.init(VAPrim::TRIANGLES);
	quad.add_quad(vec2(-1, 1), vec2(2, 2));
	overlay_quad.add_quad(vec2(0), vec2(300, 300));

	white_tex = global_textures.find_or_load("white.png");

	intermediate.create(FramebufferSpec(global_app.width, global_app.height, 1, FBAttachments::FLOAT16, true));

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	HDRbuffer.create(FramebufferSpec(global_app.width, global_app.height, 8, FBAttachments::FLOAT16, true));

	depth_map.create(FramebufferSpec(SHADOW_WIDTH, SHADOW_WIDTH, 1, FBAttachments::DEPTH, false));
	glBindTexture(GL_TEXTURE_2D, depth_map.depth_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glBindTexture(GL_TEXTURE_2D, 0);
	//glDrawBuffer(GL_NONE);
	//glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	init_bloom();

	init_basic_sphere();

	init_tiled_rendering();

	glGenFramebuffers(1, &cubemap_framebuffer);
	glGenRenderbuffers(1, &cubemap_depth_renderbuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, cubemap_framebuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, cubemap_depth_renderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 256, 256);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cubemap_depth_renderbuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenTextures(6, cubemap_textures);
	for (int i = 0; i < 6; i++) {
		glBindTexture(GL_TEXTURE_2D, cubemap_textures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glBindTexture(GL_TEXTURE_2D, 0);


	cube_with_normals.init(VertexArray::TRIANGLES);
	vec3 normals[6];
	for (int i = 0; i < 6; i++) {
		normals[i] = vec3(0);
		normals[i][i / 2] = (i & 1) ? -1.0 : 1.0;
	}
	cube_with_normals.add_solid_box(-vec3(1), vec3(1), normals);

	gen_cubemap_views();
}

void Renderer::render_scene(SceneData& scene)
{
	//calc_fruxel_visibility();
	
	Stopwatch total, world, bloom;
	total.start();
	stats = {};

	
	//glClearColor(0.5, 0.7, 1.0, 1);
	glClearColor(0,0,0, 1);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	mat4 light_space_matrix = render_shadow_map(scene);

	glViewport(0, 0, view.width,view.height);

	glEnable(GL_CULL_FACE);
	HDRbuffer.bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	projection_matrix = get_projection_matrix(), view_matrix = scene.active_camera()->view_matrix;
	
	world.start();
	world_pass(scene);
	stats.world_ms = world.end();

	if (d_enviorment_probes) {
		draw_cubemaps(&scene);
	}


	//glDisable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	primitive_debug_pass();
	glFrontFace(GL_CCW);
	
	//glEnable(GL_CULL_FACE);

	// unbind framebuffer, blit to resolve multisample
	glBindFramebuffer(GL_READ_FRAMEBUFFER, HDRbuffer.ID);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, intermediate.ID);
	glBlitFramebuffer(0, 0, view.width, view.height, 0, 0, view.width, view.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	bloom.start();
	bloom_pass();
	stats.bloom_ms = bloom.end();

	if (bloom_debug) {
		uint32_t id;
		if (show_bright_pass) {
			id = bright_pass;
		}
		else {
			if (down_sample) {
				id = downsample[!first_blur_pass][sample_num];
			}
			else {
				id = upsample[sample_num];
			}
		}
		halt_and_render_to_screen(id);
		return;
	}


	glDisable(GL_CULL_FACE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, global_app.width, global_app.height);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, intermediate.color_id);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, upsample[0]);


	gamma_tm_bloom.use();
	gamma_tm_bloom.set_float("gamma", gamma);
	gamma_tm_bloom.set_float("exposure", exposure).set_int("screen_tex", 0).set_int("bloom_tex", 1);

	quad.draw_array();




	if (d_lightmap_overlay) {

		mat4 two2d = glm::ortho(0.0f, (float)view.width, (float)-view.height, 0.0f);
		textured_prim_transform.use();
		textured_prim_transform.set_mat4("u_projection", two2d).set_mat4("u_view", mat4(1.0));
		glDisable(GL_DEPTH_TEST);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, cubemap_textures[cube_num]);
		overlay_quad.draw_array();
		glEnable(GL_DEPTH_TEST);
	}

	stats.total_ms = total.end();
}

void Renderer::upload_point_lights(SceneData& scene)
{
	point_lights.use();
	for (int i = 0; i < scene.num_lights; i++) {
		point_lights.set_vec3(("lights[" + std::to_string(i) + "].position").c_str(), scene.lights[i].position);
		point_lights.set_vec3(("lights[" + std::to_string(i) + "].color").c_str(), scene.lights[i].color);
		point_lights.set_float(("lights[" + std::to_string(i) + "].radius").c_str(), scene.lights[i].radius);
	}
}

void Renderer::world_pass(SceneData& scene)
{
	if (d_world)
	{
		lightmap_geo();
	}

	if (d_ents) {
		// Directional light pass + ambient
		/*directional_shadows.use();
		directional_shadows.set_mat4("u_projection", projection_matrix).set_mat4("u_view", view_matrix).set_vec3("light.direction", scene.sun.direction)
			.set_vec3("light.ambient", scene.sun.ambient).set_vec3("light.diffuse", scene.sun.diffuse)
			.set_vec3("light.specular", scene.sun.specular).set_int("baseTex", 0).set_int("shinyTex", 1).set_int("shadow_map", 2)
			.set_vec3("viewPos", scene.active_camera()->position).set_float("shininess", 30).set_mat4("u_light_space_matrix", light_space_matrix);

		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, depth_map.depth_id);
		scene_pass(scene, directional_shadows);
		*/
		// Point lights pass
		//upload_point_lights(scene);

		ambientcube_shade.use();
		ambientcube_shade.set_mat4("u_projection", projection_matrix).set_mat4("u_view", view_matrix)
			.set_vec3("view_pos", scene.active_camera()->position).set_int("diffuse_tex", 0).set_int("shiny_tex", 1);

		for (const auto obj : scene.objects) {
			
			if (obj->closest_reflection_probe != -1)
				continue;
			

			//for (int i = 0; i < 6; i++) {
			//	ambientcube_shade.set_vec3(("ambient_cube[" + std::to_string(i) + "]").c_str(),
			//		global_app.world.ambient_grid.at(obj->closest_ambient_cube).axis_colors[i]);
			//}




			// this should be changed, the list should only be textured items
			if (obj->has_shading) {

				// this is going to go
				if (obj->matrix_needs_update) {
					obj->update_matrix();
					obj->matrix_needs_update = false;
				}
				// Check if previous draw call used same texture or buffer!
				//Model::SubMesh& sm = obj->model->meshes.at(0);
				for (int i = 0; i < obj->model->num_meshes(); i++) {

					const RenderMesh* rm = obj->model->mesh(i);

					if (rm->diffuse) {
						rm->diffuse->bind(0);
					}
					else {
						white_tex->bind(0);
					}

					if (rm->specular) {
						rm->specular->bind(1);
					}
					else {
						white_tex->bind(1);
					}

					ambientcube_shade.set_mat4("u_model", obj->model_matrix).set_mat4("normal_mat", obj->inverse_matrix);

					//	sm.mesh.bind();
					glBindVertexArray(rm->vao);
					glDrawElements(GL_TRIANGLES, rm->num_indices, GL_UNSIGNED_INT, NULL);
					//sm.mesh.draw_indexed_primitive();

				}
			}
		}

		ambientcube_reflection.use();
		ambientcube_reflection.set_mat4("u_projection", projection_matrix).set_mat4("u_view", view_matrix)
			.set_vec3("view_pos", scene.active_camera()->position).set_int("diffuse_tex", 0).set_int("shiny_tex", 1);

		ambientcube_reflection.set_int("cubemap", 2);
	}
}


void Renderer::draw_world_geo(Shader& s)
{
	glFrontFace(GL_CW);
	Model* m = global_world.get_model();
	for (int i = 0; i < m->num_meshes(); i++) {

		const RenderMesh* rm = m->mesh(i);

		if (rm->diffuse) {
			//white_tex->bind(0);

			//rm->diffuse->bind(0);
		}
		else {
			white_tex->bind(0);
		}

		if (rm->specular) {
			//white_tex->bind(1);

			rm->specular->bind(1);
		}
		else {
			white_tex->bind(1);
		}

		s.set_mat4("u_model", mat4(1)).set_mat4("normal_mat", mat4(1));

		//	sm.mesh.bind();
		glBindVertexArray(rm->vao);
		glDrawElements(GL_TRIANGLES, rm->num_indices, GL_UNSIGNED_INT, NULL);
		//sm.mesh.draw_indexed_primitive();
	}
	glFrontFace(GL_CCW);
}
void Renderer::scene_pass(SceneData& scene, Shader& shader)
{
	for (const auto obj : scene.objects) {
		// this should be changed, the list should only be textured items
		if (obj->has_shading) {

			// this is going to go
			if (obj->matrix_needs_update) {
				obj->update_matrix();
				obj->matrix_needs_update = false;
			}

			// Frustum culling
			if (!global_app.scene->cams[0].frust.sphere_in_frustum(obj->position, 4)) {
			//	continue;
			}

			// Check if previous draw call used same texture or buffer!
			//Model::SubMesh& sm = obj->model->meshes.at(0);
			for (int i = 0; i < obj->model->num_meshes();i++) {

				const RenderMesh* rm = obj->model->mesh(i);

				if (rm->diffuse) {
					rm->diffuse->bind(0);
				}
				else {
					white_tex->bind(0);
				}

				if (rm->specular) {
					rm->specular->bind(1);
				}
				else {
					white_tex->bind(1);
				}

				shader.set_mat4("u_model", obj->model_matrix).set_mat4("normal_mat", obj->inverse_matrix);

			//	sm.mesh.bind();
				glBindVertexArray(rm->vao);
				glDrawElements(GL_TRIANGLES, rm->num_indices, GL_UNSIGNED_INT, NULL);
				//sm.mesh.draw_indexed_primitive();

			}
		}
	}
}

void Renderer::init_bloom()
{
	glGenTextures(1, &bright_pass);
	glGenTextures(12, &downsample[0][0]);
	glGenTextures(6, &upsample[0]);

	glGenFramebuffers(1, &bright_pass_fbo);
	glGenFramebuffers(12, &downsample_fbo[0][0]);
	glGenFramebuffers(6, &upsample_fbo[0]);

	glBindFramebuffer(GL_FRAMEBUFFER, bright_pass_fbo);
	glBindTexture(GL_TEXTURE_2D, bright_pass);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, global_app.width/2, global_app.height/2, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bright_pass, 0);

	int w = global_app.width / 4, h = global_app.height / 4;
	for (int i = 0; i < 6; i++) {
		glBindFramebuffer(GL_FRAMEBUFFER, downsample_fbo[0][i]);

		glBindTexture(GL_TEXTURE_2D, downsample[0][i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0,GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, downsample[0][i], 0);

		glBindFramebuffer(GL_FRAMEBUFFER, downsample_fbo[1][i]);
		glBindTexture(GL_TEXTURE_2D, downsample[1][i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, downsample[1][i], 0);


		w /= 2; h /= 2;
	}
	
	w = global_app.width / 2; h = global_app.height / 2;
	for (int i = 0; i < 6; i++) {
		glBindFramebuffer(GL_FRAMEBUFFER, upsample_fbo[i]);

		glBindTexture(GL_TEXTURE_2D, upsample[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, upsample[i], 0);
		w /= 2; h /= 2;
	}
}
void Renderer::bloom_pass()
{
	glDisable(GL_CULL_FACE);

	glBindFramebuffer(GL_FRAMEBUFFER, bright_pass_fbo);
	glClear(GL_COLOR_BUFFER_BIT);

	glViewport(0, 0, global_app.width / 2, global_app.height / 2);	// has half width + height
	bright_pass_filter.use();
	bright_pass_filter.set_float("threshold", threshold);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, intermediate.color_id);	// intermediate is the buffer that resolved multisample
	quad.draw_array();

	// "ping pong" between textures
	guassian_blur.use();
	glBindTexture(GL_TEXTURE_2D, bright_pass);

	float w = global_app.width / 4, h = global_app.height / 4;
	for (int i = 0; i < 6; i++) {
		glViewport(0, 0, w, h);

		guassian_blur.set_bool("horizontal", false);
		glBindFramebuffer(GL_FRAMEBUFFER, downsample_fbo[0][i]);
		
		// texture is set by previous
		quad.draw_array();
		
		guassian_blur.set_bool("horizontal", true);
		glBindFramebuffer(GL_FRAMEBUFFER, downsample_fbo[1][i]);

		glBindTexture(GL_TEXTURE_2D, downsample[0][i]);
		quad.draw_array();

		glBindTexture(GL_TEXTURE_2D, downsample[1][i]);

		w /= 2;
		h /= 2;
	}

	upsample_shade.use();
	upsample_shade.set_int("tex1", 0).set_int("tex2", 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, downsample[0][5]);

	w *= 4;
	h *= 4;

	for (int i = 5; i >= 0; i--) {
		glBindFramebuffer(GL_FRAMEBUFFER, upsample_fbo[i]);
		glViewport(0, 0, w, h);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, downsample[0][i]);
		quad.draw_array();

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, upsample[i]);

		w *= 2;
		h *= 2;
	}
}

mat4 Renderer::render_shadow_map(SceneData& scene)
{
	glViewport(0, 0, SHADOW_WIDTH, SHADOW_WIDTH);
	depth_map.bind();
	glClear(GL_DEPTH_BUFFER_BIT);

	float near_plane = shadow_map.near, far_plane = shadow_map.far;
	mat4 light_projection = glm::ortho(-shadow_map.width, shadow_map.width, -shadow_map.width, shadow_map.width, near_plane, far_plane);

	mat4 light_view = glm::lookAt(-scene.sun.direction*shadow_map.distance,vec3(0,0,0), vec3(0, 1, 0));
	mat4 light_space_matrix = light_projection * light_view;

	depth_render.use();
	depth_render.set_mat4("u_view_projection",light_space_matrix);

	scene_pass(scene, depth_render);


	return light_space_matrix;
}

void Renderer::halt_and_render_to_screen(uint32_t texture_id)
{
	no_transform.use();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, global_app.width, global_app.height);
	glDisable(GL_CULL_FACE);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	quad.draw_array();
	glEnable(GL_CULL_FACE);
}


void Renderer::primitive_debug_pass()
{
	// Drawn on top of final tonemapped/gamma corrected framebuffer

	transformed_primitives.use();
	transformed_primitives.set_mat4("u_projection", projection_matrix).set_mat4("u_view", view_matrix);


	glDisable(GL_DEPTH_TEST);
	glPointSize(5);
	debug_points.draw_array();
	debug_points.clear();

	glLineWidth(2);
	debug_lines.draw_array();
	debug_lines.clear();
	glLineWidth(1);
	glEnable(GL_DEPTH_TEST);

	if (d_lightmap_debug) {
		draw_lightmap_debug();
	}
	if (d_lightmap_patches) {
		draw_lightmap_patches();
	}
	if (d_trace_boxes) {
		global_world.tree.draw_trace_boxes();
	}
	if (d_tree_nodes) {
		global_world.tree.draw_tree_nodes();
	}
	if (d_world_face_edges) {
		global_world.draw_face_edges();
	}
	if (d_trace_hits) {
		global_world.draw_trace_hits();
	}

	glPointSize(1);
}
void Renderer::bounding_sphere_pass(SceneData& scene)
{
	glDisable(GL_DEPTH_TEST);

	model_primitives.use();
	model_primitives.set_mat4("u_projection", projection_matrix).set_mat4("u_view", view_matrix);
	for (auto obj : scene.objects) {
		mat4 model = mat4(1);
		model = translate(model, obj->position);
		model = scale(model, vec3(4.f));
		model_primitives.set_mat4("u_model", model);
		basic_sphere.draw_array();
	}

	glEnable(GL_DEPTH_TEST);
}
VertexP make_point(vec3 pos, vec3 color)
{
	VertexP p;
	p.r = (color.r > 1) ? 255 : color.r * 255;
	p.g = (color.g > 1) ? 255 : color.g * 255;
	p.b = (color.b > 1) ? 255 : color.b * 255;
	p.position = pos;
	return p;
}
void Renderer::debug_point(vec3 point, vec3 color)
{
	debug_points.append(make_point(point, color));
}
void Renderer::debug_box(vec3 min, vec3 max, vec3 color)
{
	vec3 corners[8] = { max, vec3(max.x,max.y,min.z),vec3(min.x,max.y,min.z),vec3(min.x,max.y,max.z),	// top CCW
						vec3(max.x,min.y,max.z), vec3(max.x,min.y,min.z),min,vec3(min.x,min.y,max.z) };	// bottom
	// top + bottom
	for (int i = 0; i < 4; i++) {
		debug_lines.push_2(make_point(corners[i], color), make_point(corners[(i + 1) % 4], color));
		debug_lines.push_2(make_point(corners[4+i], color), make_point(corners[4 + ((i + 1) % 4)], color));
	}
	// connecting
	for (int i = 0; i < 4; i++) {
		debug_lines.push_2(make_point(corners[i], color), make_point(corners[i+4], color));
	}
}
void Renderer::debug_line(vec3 start, vec3 end, vec3 color)
{
	debug_lines.push_2(make_point(start, color), make_point(end, color));
}
void Renderer::lightmap_geo()
{
	glFrontFace(GL_CW);
	lightmap.use();
	lightmap.set_mat4("u_projection", projection_matrix).set_mat4("u_view", view_matrix)
		.set_int("baseTex", 0).set_int("lightmap", 1).set_mat4("u_model",mat4(1));
	if (lightmap_nearest) {
	lightmap_tex_nearest->bind(1);
	}
	else {
	lightmap_tex_linear->bind(1);
	}
	for (int j = 0; j < stress_test_world_draw_count; j++) {
		Model* m = global_world.get_model();
		for (int i = 0; i < m->num_meshes(); i++) {

			const RenderMesh* rm = m->mesh(i);

			if (rm->diffuse && !no_textures) {
				rm->diffuse->bind(0);
			}
			else {
				white_tex->bind(0);
			}


			//shader.set_mat4("u_model", obj->model_matrix).set_mat4("normal_mat", obj->inverse_matrix);

			//	sm.mesh.bind();
			glBindVertexArray(rm->vao);
			glDrawElements(GL_TRIANGLES, rm->num_indices, GL_UNSIGNED_INT, NULL);
			stats.draw_calls++;
			stats.tris += rm->num_indices / 3;
			//sm.mesh.draw_indexed_primitive();

		}
	}
	glFrontFace(GL_CCW);

}
void Renderer::init_basic_sphere()
{
	basic_sphere.init(VAPrim::LINES);
	const int iterations = 50;
	const float angle_delta = 3.14159 * 2 / iterations;
	vec3 points[iterations];
	for (int i = 0; i < iterations; i++) {
		points[i] = vec3(cos(i*angle_delta), 0, sin(i*angle_delta));
	}
	for (int i = 0; i < iterations-1; i++) {
		basic_sphere.push_2(VertexP(points[i], vec3(0, 1, 0)), VertexP(points[i + 1], vec3(0, 1, 0)));
	}
	basic_sphere.push_2(VertexP(points[iterations - 1], vec3(0, 1, 0)), VertexP(points[0], vec3(0, 1, 0)));

	for (int i = 0; i < iterations; i++) {
		points[i] = vec3(0, cos(i * angle_delta), sin(i * angle_delta));
	}
	for (int i = 0; i < iterations - 1; i++) {
		basic_sphere.push_2(VertexP(points[i], vec3(1, 0, 0)), VertexP(points[i + 1], vec3(1, 0, 0)));
	}
	basic_sphere.push_2(VertexP(points[iterations - 1], vec3(1, 0, 0)), VertexP(points[0], vec3(1, 0, 0)));

	for (int i = 0; i < iterations; i++) {
		points[i] = vec3(sin(i * angle_delta), cos(i * angle_delta), 0);
	}
	for (int i = 0; i < iterations - 1; i++) {
		basic_sphere.push_2(VertexP(points[i], vec3(0, 0, 1)), VertexP(points[i + 1], vec3(0, 0, 1)));
	}
	basic_sphere.push_2(VertexP(points[iterations - 1], vec3(0, 0, 1)), VertexP(points[0], vec3(0, 0, 1)));
}
void Renderer::visualize_overdraw(SceneData& scene)
{
	overdraw.use();
	overdraw.set_mat4("u_projection", projection_matrix).set_mat4("u_view", view_matrix);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	scene_pass(scene, overdraw);
	glDisable(GL_BLEND);
}
void Renderer::load_shaders()
{
	textured_prim_transform.load_from_file("textured_primitive_v.txt", "textured_primitive_f.txt");
	debug_depth.load_from_file("no_transform_v.txt", "depth_map_debug_f.txt");

	depth_render.load_from_file("depth_render_v.txt", "depth_render_f.txt");

	point_lights = Shader("point_light_v.txt", "point_light_f.txt");

	gamma_tm = Shader("no_transform_v.txt", "gamma_f.txt");
	gamma_tm_bloom = Shader("no_transform_v.txt", "gamma_bloom_f.txt");


	bright_pass_filter = Shader("no_transform_v.txt", "bright_pass_filter_f.txt");

	guassian_blur = Shader("no_transform_v.txt", "guassian_blur_f.txt");
	upsample_shade = Shader("no_transform_v.txt", "upsample_f.txt");

	no_transform = Shader("no_transform_v.txt", "no_transform_f.txt");

	directional_shadows = Shader("directional_shadows_v.txt", "directional_shadows_f.txt");

	transformed_primitives = Shader("transform_p_v.txt", "transform_p_f.txt");
	model_primitives = Shader("transform_pmodel_v.txt", "transform_pmodel_f.txt");

	fresnel = Shader("point_light_v.txt", "fresnel_f.txt");
	overdraw = Shader("point_light_v.txt", "overdraw_f.txt");

	lightmap = Shader("lightmap_generic_v.txt", "lightmap_generic_f.txt");

	forward_plus.load_from_file("directional_shadows_v.txt", "forward_plus_f.txt");

	ambientcube_shade.load_from_file("point_light_v.txt", "ambient_cube_f.txt");

	ambientcube_reflection.load_from_file("point_light_v.txt", "AmbientCubeReflection_f.txt");

	cubemap_shader.load_from_file("transform_pmodel_v.txt", "ReflectionProbe_f.txt");
}
mat4 Renderer::get_projection_matrix()
{
	return glm::perspective(glm::radians(view.fov_y), (float)view.width / (float)view.height, view.znear, view.zfar);
}

void Renderer::update_world_cubemaps(SceneData* scene)
{
	for (int i = 0; i < scene->enviorment_probes.size(); i++) {
		EnviormentProbe* ep = &scene->enviorment_probes.at(i);
		if (ep->cubemap_index == -1) {
			ep->cubemap_index = world_cubemaps.size();
			Cubemap cm;
			cm.init_empty(256);
			world_cubemaps.push_back(cm);
		}

		render_to_cubemap(ep, scene);
	}
}


static mat4 cubemap_views[6];
void gen_cubemap_views()
{
	vec3 front, right, up;

	for (int i = 0; i < 6; i++)
	{
		// From Khronos wiki
		switch (i)
		{

		case 0:	// +X
			front = vec3(1, 0, 0);
			up = vec3(0, -1, 0);
			right = vec3(0, 0, -1);
			break;
		case 1:	// -X
			front = vec3(-1, 0, 0);
			up = vec3(0, -1, 0);
			right = vec3(0, 0, 1);
			break;
		case 2: // +Y
			front = vec3(0, 1, 0);
			up = vec3(0, 0, 1);
			right = vec3(1, 0, 0);
			break;
		case 3: // -Y
			front = vec3(0, -1, 0);
			up = vec3(0, 0, -1);
			right = vec3(1, 0, 0);
			break;
		case 4:
			front = vec3(0, 0, 1);
			up = vec3(0, -1, 0);
			right = vec3(1, 0, 0);
			break;
		case 5:
			front = vec3(0, 0, -1);
			up = vec3(0, -1, 0);
			right = vec3(-1, 0, 0);
			break;
		}
		cubemap_views[i] = mat4(1);

		cubemap_views[i][0][0] = right.x;
		cubemap_views[i][1][0] = right.y;
		cubemap_views[i][2][0] = right.z;
		cubemap_views[i][0][1] = up.x;
		cubemap_views[i][1][1] = up.y;
		cubemap_views[i][2][1] = up.z;
		cubemap_views[i][0][2] = -front.x;
		cubemap_views[i][1][2] = -front.y;
		cubemap_views[i][2][2] = -front.z;
	}

}


void Renderer::render_to_cubemap(const EnviormentProbe* ep, SceneData* scene)
{
	projection_matrix = glm::perspective(glm::radians(90.0), 1.0, 0.5, 100.0);
	
	glBindFramebuffer(GL_FRAMEBUFFER, cubemap_framebuffer);
	glViewport(0, 0, 256, 256);

	Cubemap* cm = &world_cubemaps.at(ep->cubemap_index);

	for (int i = 0; i < 6; i++)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cm->get_ID(), 0);
		//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cubemap_textures[i], 0);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		/*
		vec3 front = vec3(0);
		front[i / 2] = (i & 1) ? -1.f : 1.f;
		
		vec3 up = vec3(0, 1, 0);
		if (i / 2 == 1) {
			up = vec3((i & 1) ? 1.0 : -1.0, 0, 0);
		}
		*/
		// Set translation part of view matrix
		vec3 side = vec3(cubemap_views[i][0][0], cubemap_views[i][1][0], cubemap_views[i][2][0]);
		vec3 up = vec3(cubemap_views[i][0][1], cubemap_views[i][1][1], cubemap_views[i][2][1]);
		vec3 front = vec3(cubemap_views[i][0][2], cubemap_views[i][1][2], cubemap_views[i][2][2]);

		view_matrix = cubemap_views[i];

		view_matrix[3][0] = -dot(side, ep->position);
		view_matrix[3][1] = -dot(up, ep->position);
		view_matrix[3][2] = -dot(front, ep->position);


		//view_matrix = glm::lookAt(ep->position, ep->position + front, up);

		world_pass(*scene);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::draw_cubemaps(SceneData* scene)
{
	glDisable(GL_CULL_FACE);
	cubemap_shader.use();
	cubemap_shader.set_mat4("u_projection", projection_matrix).set_mat4("u_view", view_matrix);
	cubemap_shader.set_vec3("ViewPos", scene->active_camera()->position);
	for (int i = 0; i < scene->enviorment_probes.size(); i++)
	{
		const auto& ep = scene->enviorment_probes[i];
		if (ep.cubemap_index == -1 || ep.dont_draw)
			continue;
		mat4 model = mat4(1.f);

		model = translate(model, ep.position);
		cubemap_shader.set_mat4("u_model", model);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, world_cubemaps.at(ep.cubemap_index).get_ID());
		//glBindTexture(GL_TEXTURE_2D, cubemap_textures[cube_num]);

		cube_with_normals.draw_array();
	}
	glEnable(GL_CULL_FACE);

}