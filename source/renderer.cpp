#include "renderer.h"
#include "loader.h"
#include "app.h"
#include "types.h"
#include "camera.h"
#include "glad/glad.h"

#include "Texture_def.h"
#include "TextureManager.h"

#include <iostream>

const int SHADOW_WIDTH = 2048;

void Renderer::on_resize()
{
	HDRbuffer.update_window_size(app.width, app.height);
	intermediate.update_window_size(app.width, app.height);
}
Renderer::Renderer(App& app): app(app)
{
	debug_depth = Shader("no_transform_v.txt", "depth_map_debug_f.txt");

	depth_render = Shader("depth_render_v.txt", "depth_render_f.txt");

	untextured = Shader("basic_lighting_vr.txt", "basic_lighting_fr.txt");
	untextured_unshaded = Shader("light_cube_vr.txt", "light_cube_fr.txt");
	textured_mesh = Shader("directional.vert", "directional.frag");

	point_lights = Shader("point_light_v.txt", "point_light_f.txt");

	//cube = load_qobj_mesh("cube.qobj");
	sky_gradient = global_textures.find_or_load("sky_gradient2.png");

	sky_shader = Shader("gradient.vert", "gradient.frag");

	gamma_tm = Shader("no_transform_v.txt", "gamma_f.txt");
	gamma_tm_bloom = Shader("no_transform_v.txt", "gamma_bloom_f.txt");


	bright_pass_filter = Shader("no_transform_v.txt", "bright_pass_filter_f.txt");

	guassian_blur = Shader("no_transform_v.txt", "guassian_blur_f.txt");
	upsample_shade = Shader("no_transform_v.txt", "upsample_f.txt");

	no_transform = Shader("no_transform_v.txt", "no_transform_f.txt");

	directional_shadows = Shader("directional_shadows_v.txt", "directional_shadows_f.txt");

	transformed_primitives = Shader("transform_p_v.txt", "transform_p_f.txt");
	model_primitives = Shader("transform_pmodel_v.txt", "transform_pmodel_f.txt");

	leaf = Shader("leaf_shader_v.txt", "directional.frag");

	fresnel = Shader("point_light_v.txt", "fresnel_f.txt");

	debug_lines.set_primitive(VertexArray::Primitive::lines);
	debug_points.set_primitive(VertexArray::Primitive::points);



	quad.add_quad(vec2(-1, 1), vec2(2, 2));
	temp = global_textures.find_or_load("engineer_red.png");

	white_tex = global_textures.find_or_load("white.png");

	intermediate.create(FramebufferSpec(app.width, app.height, 1, FBAttachments::a_float16, true));

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	new_b.create(FramebufferSpec(app.width, app.height, 4, FBAttachments::a_rgb, true));


	HDRbuffer.create(FramebufferSpec(app.width, app.height, 4, FBAttachments::a_float16, true));

	depth_map.create(FramebufferSpec(SHADOW_WIDTH, SHADOW_WIDTH, 1, FBAttachments::a_depth, false));
	glBindTexture(GL_TEXTURE_2D, depth_map.depth_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glBindTexture(GL_TEXTURE_2D, 0);
	//glDrawBuffer(GL_NONE);
	//glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	init_bloom();

	init_basic_sphere();


}

void Renderer::render_scene(SceneData& scene)
{
	// BIND FRAMEBUFFER

	//depth_map.bind();
	// Clears default buffer
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	mat4 light_space_matrix = render_shadow_map(scene);

	halt_and_render_to_screen(temp->get_ID());
	//return;

	glViewport(0, 0, app.width, app.height);
	debug_depth.use();
	glDisable(GL_CULL_FACE);
	glBindTexture(GL_TEXTURE_2D, depth_map.depth_id);
	quad.draw_array();
	glEnable(GL_CULL_FACE);

	//return;
	HDRbuffer.bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//draw_gradient_skybox(scene);

	projection_matrix = app.projection_matrix, view_matrix = scene.active_camera()->view_matrix;
	
	untextured_unshaded.use();
	untextured_unshaded.set_mat4("projection", projection_matrix).set_mat4("view", view_matrix);

	// Unshaded untextured
	/*
	for (auto obj : scene.objects) {
		if (!obj->has_shading && !obj->model->meshes.at(0).mat.diffuse) {

			// this is going to go
			if (obj->matrix_needs_update) {
				obj->update_matrix();
				obj->matrix_needs_update = false;
			}

			untextured_unshaded.set_mat4("model", obj->model_matrix);
			Model::SubMesh& sm = obj->model->meshes.at(0);

			obj->model->meshes.at(0).mesh.bind();

			obj->model->meshes.at(0).mesh.draw_indexed_primitive();
		}
	}
	*/

	// Temporary fresnel pass
	//fresnel.use();
	//fresnel.set_mat4("u_projection", projection_matrix).set_mat4("u_view", view_matrix).set_vec3("view_pos", scene.active_camera()->position)
	//	.set_float("opacity", 0.1f).set_vec3("rim_color", vec3(1.0, 0.2, 0.5)).set_vec3("interior_color", vec3(0, 0, 0));
	//scene_pass(scene, fresnel);
	
	//goto skip_light;


	// Directional light pass + ambient
	directional_shadows.use();
	directional_shadows.set_mat4("u_projection", projection_matrix).set_mat4("u_view", view_matrix).set_vec3("light.direction", scene.sun.direction)
		.set_vec3("light.ambient", scene.sun.ambient).set_vec3("light.diffuse", scene.sun.diffuse)
		.set_vec3("light.specular", scene.sun.specular).set_int("baseTex", 0).set_int("shinyTex", 1).set_int("shadow_map",2)
		.set_vec3("viewPos", scene.active_camera()->position).set_float("shininess", 30).set_mat4("u_light_space_matrix", light_space_matrix);

	glActiveTexture(GL_TEXTURE0 + 2);
	glBindTexture(GL_TEXTURE_2D, depth_map.depth_id);

	scene_pass(scene, directional_shadows);

	// Point lights pass
	upload_point_lights(scene);
	point_lights.set_mat4("u_projection", projection_matrix).set_mat4("u_view", view_matrix)
		.set_vec3("view_pos", scene.active_camera()->position).set_int("diffuse_tex", 0).set_int("shiny_tex", 1);
	// Enable additive blending
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	scene_pass(scene, point_lights);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	
skip_light:

	// unbind framebuffer, blit to resolve multisample
	glBindFramebuffer(GL_READ_FRAMEBUFFER, HDRbuffer.ID);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, intermediate.ID);
	glBlitFramebuffer(0, 0, app.width, app.height, 0, 0, app.width, app.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	bloom_pass();
	// ugly ass shit
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
	// Draw to the screen
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, app.width, app.height);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, intermediate.color_id);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, upsample[0]);


	gamma_tm_bloom.use();
	gamma_tm_bloom.set_float("gamma", gamma);
	gamma_tm_bloom.set_float("exposure", exposure).set_int("screen_tex", 0).set_int("bloom_tex", 1);

	quad.draw_array();

	primitive_debug_pass();
	//bounding_sphere_pass(scene);

	glEnable(GL_CULL_FACE);

}

void Renderer::draw_gradient_skybox(SceneData& scene)
{
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	sky_shader.use();
	sky_shader.set_vec3("sun_direction", scene.sun.direction);
	sky_shader.set_mat4("projection", app.projection_matrix);
	sky_shader.set_mat4("view", mat3(scene.active_camera()->view_matrix));	// Removes translation from matrix
	//sky_gradient.bind(0);
	//cube.bind();
	//cube.draw_indexed_primitive();
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
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
			if (!app.scene->cams[0].frust.sphere_in_frustum(obj->position, 4)) {
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app.width/2, app.height/2, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bright_pass, 0);

	int w = app.width / 4, h = app.height / 4;
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
	
	w = app.width / 2; h = app.height / 2;
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

	glViewport(0, 0, app.width / 2, app.height / 2);	// has half width + height
	bright_pass_filter.use();
	bright_pass_filter.set_float("threshold", threshold);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, intermediate.color_id);	// intermediate is the buffer that resolved multisample
	quad.draw_array();

	// "ping pong" between textures
	guassian_blur.use();
	glBindTexture(GL_TEXTURE_2D, bright_pass);

	float w = app.width / 4, h = app.height / 4;
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

//vec3 pivot 
//	mat4 angle_rotation
//	glm::rotate()

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
	glViewport(0, 0, app.width, app.height);
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
	glPointSize(1);

	glLineWidth(1);
	debug_lines.draw_array();
	debug_lines.clear();

	glEnable(GL_DEPTH_TEST);

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
void Renderer::add_point(vec3 point, vec3 color)
{
	debug_points.append(make_point(point, color));
}
void Renderer::add_line(vec3 start, vec3 end, vec3 color)
{
	debug_lines.push_2(make_point(start, color), make_point(end, color));
}
void Renderer::init_basic_sphere()
{
	// x
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

	basic_sphere.set_primitive(VertexArray::lines);
}