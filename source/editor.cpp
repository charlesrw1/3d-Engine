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

#include <iostream>


Editor::Editor()
{
}
#define R_BOOLEAN_IMGUI(str, var) 	if (ImGui::RadioButton(str, r->var)) { \
r->var = !r->var; \
}

void Editor::on_render()
{
	ImGui_ImplSDL2_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();
	
	Renderer* r = global_app.r;
	
	const Renderstats* rs = &r->stats;
	ImGui::Begin("Stats");
	ImGui::Text("Render MS  : %f", rs->total_ms);
	ImGui::Text("World MS   : %f", rs->world_ms);
	ImGui::Text("Bloom MS   : %f", rs->bloom_ms);
	ImGui::Text("Draw calls : %d", rs->draw_calls);
	ImGui::Text("Tris       : %d", rs->tris);
	ImGui::End();





	ImGui::Begin("Editor");


	ImGui::InputText("Enter map name", &buffer[0], 256);
	if (ImGui::Button("Load new map")) {
			global_app.free_current_map();
			global_app.load_map(buffer);
			global_app.setup_new_map();
	}


	R_BOOLEAN_IMGUI("d_world", d_world);
	R_BOOLEAN_IMGUI("d_world_face_edges", d_world_face_edges);
	R_BOOLEAN_IMGUI("d_trace_boxes", d_trace_boxes);
	R_BOOLEAN_IMGUI("d_tree_nodes", d_tree_nodes);
	R_BOOLEAN_IMGUI("d_trace_hits", d_trace_hits);
	R_BOOLEAN_IMGUI("d_lightmap_patches", d_lightmap_patches);
	R_BOOLEAN_IMGUI("d_lightmap_debug", d_lightmap_debug);
	R_BOOLEAN_IMGUI("lightmap_nearest", lightmap_nearest);
	R_BOOLEAN_IMGUI("d_lightmap_overlay", d_lightmap_overlay);
	R_BOOLEAN_IMGUI("no_textures", no_textures);

	ImGui::SliderInt("stress test world calls", &r->stress_test_world_draw_count, 0, 25);

	ImGui::SliderInt("cube_num", &r->cube_num, 0, 5);


	if (ImGui::Button("Reload Gamma/Tonemap shader")) {
		// Temporary, should delete shader
		global_app.r->gamma_tm_bloom = Shader("no_transform_v.txt", "gamma_bloom_f.txt");
	}
	if (ImGui::Button("Reload Directional shader")) {
		// Temporary, should delete shader
		global_app.r->directional_shadows = Shader("directional_shadows_v.txt", "directional_shadows_f.txt");
	}
	if (ImGui::RadioButton("Bloom debug", global_app.r->bloom_debug)) {
		global_app.r->bloom_debug = !global_app.r->bloom_debug;
	}
	if (ImGui::RadioButton("Bright pass", global_app.r->show_bright_pass)) {
		global_app.r->show_bright_pass = !global_app.r->show_bright_pass;
	}
	ImGui::SliderInt("Sample Num", &global_app.r->sample_num, 0, 5);
	if (ImGui::RadioButton("First pass", global_app.r->first_blur_pass)) {
		global_app.r->first_blur_pass = !global_app.r->first_blur_pass;
	}
	if (ImGui::RadioButton("Downsample", global_app.r->down_sample)) {
		global_app.r->down_sample = !global_app.r->down_sample;
	}

	ImGui::DragFloat("Gamma: ", &global_app.r->gamma, 0.1, 0.5, 4);
	ImGui::DragFloat("Exposure: ", &global_app.r->exposure, 0.1, 0, 10);
	ImGui::DragFloat("Threshold: ", &global_app.r->threshold, 0.1, 0.5, 10);
	ImGui::Image(reinterpret_cast<ImTextureID>(global_app.r->depth_map.depth_id), ImVec2(256, 256));
	ImGui::Text("Shadow Map Settings");
	ImGui::DragFloat("Near Plane:", &global_app.r->shadow_map.near, 1, 0, 100);
	ImGui::DragFloat("Far Plane:", &global_app.r->shadow_map.far, 1, 0, 100);
	ImGui::DragFloat("Ortho width:", &global_app.r->shadow_map.width, 1, 10, 50);
	ImGui::DragFloat("Distance:", &global_app.r->shadow_map.distance, 1, 10, 100);


	ImGui::Image(reinterpret_cast<ImTextureID>(global_app.r->bright_pass), ImVec2(256, 256));
	static int downsample_selection = 0;
	ImGui::SliderInt("Downsample #:", &downsample_selection, 0, 5);
	ImGui::Image(reinterpret_cast<ImTextureID>(global_app.r->downsample[1][downsample_selection]), ImVec2(256, 256));




	ImGui::End();

	for (const auto& hit : world_hits) {
		global_app.r->debug_point(hit.end_pos, vec3(1.f));
		global_app.r->debug_line(hit.end_pos, hit.end_pos + hit.normal, vec3(0.5, 1.0, 0.0));
	}

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
				global_app.running = false;
				break;
			case SDLK_1:
				cam_num = (cam_num + 1) % 2;
				global_app.scene->cam_num = cam_num;
				break;
			case SDLK_SPACE:
				shoot_ray();
				break;
			case SDLK_q:
				global_app.scene->active_camera()->toggle_scroll_state();
				break;
			}
			break;
		case SDL_MOUSEMOTION:
			if (game_focused) {
				global_app.scene->active_camera()->mouse_update(event.motion.xrel, event.motion.yrel);
			}
			break;
		case SDL_MOUSEWHEEL:
			if (game_focused) {
				global_app.scene->active_camera()->scroll_wheel_update(event.wheel.y);
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
		global_app.scene->active_camera()->keyboard_update(SDL_GetKeyboardState(0));
	}
	//shoot_ray();
}
#include <chrono>
void print_out_area(int face_num)
{
	const worldmodel_t* world = global_world.wm;
	assert(face_num < world->faces.size());
	const face_t* face = &world->faces[face_num];
	winding_t winding;
	for (int i = 0; i < face->v_count; i++) {
		winding.add_vert(world->verts[face->v_start + i]);
	}
	std::cout << "Face area: " << winding.get_area() << '\n';
}

void Editor::shoot_ray()
{
	ray_t ray;
	ray.dir = normalize(global_app.scene->active_camera()->front);
	ray.length = 100.f;
	ray.origin = global_app.scene->active_camera()->position;

	trace_t result;
	
	auto start2 = std::chrono::steady_clock::now();
	for (int i = 0; i <1; i++) {
		result = global_world.brute_force_raycast(ray);
	}
	auto end2 = std::chrono::steady_clock::now();
	auto elapsed2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
	
	result = global_world.tree.test_ray_debug(ray.origin,ray.origin+ray.dir*100.f);
	result = global_world.tree.test_ray(ray.origin, ray.origin + ray.dir * 100.f, 0.005f);
	result = global_world.tree.test_ray_fast(ray.origin, ray.origin + ray.dir * 100.f, -0.005f,0.005f);
	auto start3 = std::chrono::steady_clock::now();
	for (int i = 0; i <0; i++) {
		result = global_world.tree.test_ray(ray.origin, ray.origin + ray.dir * 100.f);
	}
	auto end3 = std::chrono::steady_clock::now();
	auto elapsed3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start3);
	world_hits.push_back(result);
	//result = {};


	auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < 0; i++) {
		result = global_world.tree.test_ray_fast(ray.origin, ray.origin + ray.dir * 100.f);
	}
	auto end = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>( end - start);
	//world_hits.push_back(result);



//	global_world.tree.print_trace_stats();

	if (1) {
		std::cout << "Ray hit: (" << elapsed3.count() / (1000.f) << " ms) (" << elapsed.count() / (1000.f) << " ms prev) (" << elapsed2.count() / (1000.f)  << " ms brute force)\n"
			<< "	Node: " << result.node << ' ' << " Face: " << result.face << '\n'
			<< "	Pos: " << result.end_pos.x << ' ' << result.end_pos.y << ' ' << result.end_pos.z << '\n'
			<< "	Length: " << result.length << '\n'
			<< "	Origin: " << result.start.x << ' ' << result.start.y << ' ' << result.start.z << '\n';
		if (result.hit) {
			print_out_area(result.face);
		}
		
		//world_hits.push_back(result);
	}
}