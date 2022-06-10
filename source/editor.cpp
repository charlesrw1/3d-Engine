#include "editor.h"
#include "app.h"
#include "imgui.h";
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"

#include <SDL2/SDL.h>

#include "types.h"
#include "opengl_api.h"
#include "renderer.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"


Editor::Editor(App& app) : app(app)
{
}
void Editor::on_render()
{
	ImGui_ImplSDL2_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();


	ImGui::Begin("ENVIORNMENT");
	
	ImGui::ColorEdit3("Sun Diffuse Light: ", &app.scene->sun.diffuse.x);


	ImGui::BeginChild("RENDERER");
	if (ImGui::Button("Reload Gamma/Tonemap shader")) {
		// Temporary, should delete shader
		app.r->gamma_tm_bloom = Shader("no_transform_v.txt", "gamma_bloom_f.txt");
	}
	if (ImGui::Button("Reload Directional shader")) {
		// Temporary, should delete shader
		app.r->directional_shadows = Shader("directional_shadows_v.txt", "directional_shadows_f.txt");
	}

	if (ImGui::RadioButton("Bloom debug", app.r->bloom_debug)) {
		app.r->bloom_debug = !app.r->bloom_debug;
	}
	if (ImGui::RadioButton("Bright pass", app.r->show_bright_pass)) {
		app.r->show_bright_pass = !app.r->show_bright_pass;
	}
	ImGui::SliderInt("Sample Num", &app.r->sample_num, 0, 5);
	if (ImGui::RadioButton("First pass", app.r->first_blur_pass)) {
		app.r->first_blur_pass = !app.r->first_blur_pass;
	}
	if (ImGui::RadioButton("Downsample", app.r->down_sample)) {
		app.r->down_sample = !app.r->down_sample;
	}


	ImGui::DragFloat("Gamma: ", &app.r->gamma, 0.1, 0.5, 4);
	ImGui::DragFloat("Exposure: ", &app.r->exposure, 0.1, 0, 10);
	ImGui::DragFloat("Threshold: ", &app.r->threshold, 0.1, 0.5, 10);
	ImGui::Image(reinterpret_cast<ImTextureID>(app.r->depth_map.depth_id), ImVec2(256, 256));
	ImGui::Text("Shadow Map Settings");
	ImGui::DragFloat("Near Plane:", &app.r->shadow_map.near, 1, 0, 100);
	ImGui::DragFloat("Far Plane:", &app.r->shadow_map.far, 1, 0, 100);
	ImGui::DragFloat("Ortho width:", &app.r->shadow_map.width, 1, 10, 50);
	ImGui::DragFloat("Distance:", &app.r->shadow_map.distance, 1, 10, 100);


	ImGui::Image(reinterpret_cast<ImTextureID>(app.r->bright_pass), ImVec2(256, 256));
	static int downsample_selection = 0;
	ImGui::SliderInt("Downsample #:", &downsample_selection,0,5);
	ImGui::Image(reinterpret_cast<ImTextureID>(app.r->downsample[1][downsample_selection]), ImVec2(256, 256));



	ImGui::EndChild();



	ImGui::End();


	/*
	ImGui::Begin("ANGLE TESTING");
	{
		static float angle = 0;

		SceneData& scene = *app.scene;
		GameObject* engie = app.scene->objects.at(1);
		angle = scene.wind_strength * sin(scene.wind_speed * SDL_GetTicks()/1000.f);

		mat4 model_matrix;
		model_matrix = mat4(1);
		//model_matrix = translate(model_matrix, scene.pivot);
		model_matrix *= glm::rotate(mat4(1), angle, cross(vec3(scene.wind_dir.x,0,scene.wind_dir.y),vec3(0,1,0)));
		//model_matrix = translate(model_matrix, -scene.pivot);


		model_matrix = glm::scale(model_matrix, vec3(engie->scale));
		model_matrix = translate(model_matrix, engie->position);

		//engie->inverse_matrix = inverse(model_matrix);
		//engie->model_matrix = model_matrix;



		ImGui::DragFloat2("Direction of \"wind\"", &app.scene->wind_dir.x,0.05, -1, 1);
		app.scene->wind_dir = normalize(app.scene->wind_dir);

		ImGui::SliderFloat("Angle", &angle, -3.14, 3.14);
		ImGui::DragFloat("Wind strength", &app.scene->wind_strength, 0.1, 0, 3);
		ImGui::DragFloat("Wind speed", &app.scene->wind_speed, 0.1, 0, 20);


		app.r->add_line(vec3(0,1,0), vec3(app.scene->wind_dir.x, 1, app.scene->wind_dir.y), vec3(0.9, 0.9, 0));
		app.r->add_line(vec3(0), engie->position, vec3(0, 0, 1));

	}
	ImGui::End;
	*/
	app.r->add_line(vec3(0), vec3(2, 0, 0), vec3(1, 0, 0));
	app.r->add_line(vec3(0), vec3(0, 2, 0), vec3(0, 1, 0));
	app.r->add_line(vec3(0), vec3(0, 0, 2), vec3(0, 0, 1));


	

	vec3 light_origin = app.r->shadow_map.distance * -app.scene->sun.direction;
	vec3 shadow_map_start = light_origin + app.scene->sun.direction * app.r->shadow_map.near;
	vec3 shadow_map_end = light_origin + app.scene->sun.direction * app.r->shadow_map.far;

	vec3 side_vec = normalize(cross(app.scene->sun.direction, vec3(0, 1, 0)));
	vec3 up_vec = normalize(cross(side_vec,app.scene->sun.direction));

	vec3 corners[8];
	
	// Draws shadow map projection matrix
	// Front quad
	if (show_shadow_map) {
		float width = app.r->shadow_map.width;
		corners[0] = shadow_map_start + (up_vec * width) - (side_vec * width);
		corners[1] = shadow_map_start + up_vec * width + side_vec * width;
		corners[2] = shadow_map_start - up_vec * width + side_vec * width;
		corners[3] = shadow_map_start - up_vec * width - side_vec * width;

		app.r->add_line(corners[0], corners[1], vec3(1, 0, 0));
		app.r->add_line(corners[1], corners[2], vec3(1, 0, 0));
		app.r->add_line(corners[2], corners[3], vec3(1, 0, 0));
		app.r->add_line(corners[3], corners[0], vec3(1, 0, 0));


		// back quad
		app.r->add_line(shadow_map_start, shadow_map_end, vec3(1.0, 0.0, 0));
	}
	//app.r->add_line(shadow_map_start, shadow_map_start + side_vec * 3.f, vec3(0, 0, 1));
	//app.r->add_line(shadow_map_start, shadow_map_start + up_vec * 3.f, vec3(0, 1, 0));
	if (show_light_lines) {

		for (int i = 0; i < app.scene->num_lights; i++) {
			app.r->add_line(vec3(0), app.scene->lights[i].position, app.scene->lights[i].color);
			app.r->add_point(app.scene->lights[i].position, app.scene->lights[i].color);
		}
	}
	
	Camera* c = &app.scene->cams[0];
	c->frust.update(*c);
	for (int i = 0; i < 5; i++) {
		app.r->add_line(vec3(0), c->frust.normals[i] * c->frust.offsets[i], vec3(i / 5.f, i/5.f, .5));
	}

	app.r->add_line(vec3(0), 10.f*-app.scene->sun.direction, vec3(1, 0, 0));

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
void Editor::handle_event(SDL_Event& event)
{

	switch (event.type)
	{
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym)
			{
			case SDLK_ESCAPE:
				app.running = false;
				break;
			case SDLK_1:
				cam_num = (cam_num + 1) % 2;
				app.scene->cam_num = cam_num;
				break;
			}
			break;
		case SDL_MOUSEMOTION:
			if (game_focused) {
				app.scene->active_camera()->mouse_update(event.motion.xrel, event.motion.yrel);
			}
			break;
		case SDL_MOUSEWHEEL:
			if (game_focused) {
				app.scene->active_camera()->scroll_wheel_update(event.wheel.y);
				app.update_projection_matrix();
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			game_focused = true;
			SDL_SetRelativeMouseMode(SDL_TRUE);
			break;
		case SDL_MOUSEBUTTONUP:
			game_focused = false;
			SDL_SetRelativeMouseMode(SDL_FALSE);
			break;
		default:
			break;
	}

}
void Editor::on_update()
{
	if (game_focused) {
		app.scene->active_camera()->keyboard_update(SDL_GetKeyboardState(0));
	}
}