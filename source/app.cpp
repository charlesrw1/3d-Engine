#include "app.h"
#include "renderer.h"
#include "types.h"
#include <SDL2/SDL.h>
#include "loader.h"
#include "glad/glad.h"
#include "glm/gtc/matrix_transform.hpp"


#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"

#include "editor.h"

#include "ModelManager.h"

#include "TextureManager.h"

#include "MapParser.h"

App global_app;

void App::init()
{
	init_window();

	r = new Renderer;
	scene = new SceneData;
	editor = new Editor;

	create_scene();
	update_projection_matrix();
}

void sdldie(const char* msg)
{
	printf(" % s: % s\n", msg, SDL_GetError());
	SDL_Quit();
	exit(1);
}
/*
GameObject* temp_create(const char* qobj, const char* texture)
{
	Mesh mes = load_qobj_mesh(qobj);
	Material mat;
	mat.diffuse = new Texture(load_texture_file(texture));
	return new GameObject(mes, mat);
}*/
vec3 make_color(uint8_t r, uint8_t b, uint8_t g) {
	return vec3(r / 255.f, b / 255.f, g / 255.f);
}

void App::create_scene()
{
	MapParser mp;
	mp.start_file("resources/maps/test.map");
	exit(1);

	u32 start = SDL_GetTicks();

	
	// yes memory leaks galore
	//scene->objects.push_back(temp_create("plane.qobj", "grid0.png"));
	//scene->objects.back()->scale = 20;
/*	scene->objects.push_back(temp_create("rep_inf_ep3trooper.qobj", "rep_inf_ep3trooper.png"));
	// Engineer
	GameObject* tmp = temp_create("engineer.qobj", "engineer_red.png");
	tmp->model->meshes.at(0).mat.specular = new Texture(load_texture_file("engineer_exponent.png"));

	for (int i = 0; i < 1; i++)
	{
		GameObject* eng = new GameObject(*tmp);
		eng->position = vec3(-3,0,2);
	//	eng->euler_y = 3.141 / 4;
		scene->objects.push_back(eng);
	}
	*/

	GameObject* temp;
	/*
	Model* plane = new Model;
	Texture* grid = new Texture;
	*grid = load_texture_file("grid0.png");
	plane->meshes.push_back(load_qobj_mesh("plane.qobj"));
	plane->meshes.at(0).mat.diffuse = grid;
	plane->meshes.at(0).mat.pong_exp = 10;

	tmp = new GameObject(plane);
	tmp->scale = vec3(20.f);
	//tmp->euler_x = 3.14 / 5;
	scene->objects.push_back(tmp);


	// Cube
	Model* cube = new Model;
	cube->meshes.push_back(load_qobj_mesh("cube.qobj"));

	tmp = new GameObject(cube);
	tmp->color = vec3(1.0f, 0.5f, 0.31f);
	tmp->position = vec3(8.f, 0.f, -4.f);
	tmp->scale = vec3(1.f, 10.f, 7.f);
	scene->objects.push_back(tmp);

	tmp = new GameObject(cube);
	tmp->color = vec3(1.f);
	tmp->scale = vec3(0.7f);
	tmp->position = vec3(2, 6, -6);
	tmp->has_shading = false;
	scene->objects.push_back(tmp);
	*/


	scene->sun.diffuse = vec3(1);//1.f*vec3(make_color(248, 197, 139));
	scene->sun.ambient = vec3(0.2,0.2,0.2);//0.0f*vec3(make_color(142, 154, 185));
	//scene->sun.diffuse = vec3(50, 45, 15);

	scene->sun.specular = vec3(1);
	scene->sun.direction = normalize(vec3(0, -1, 0));
	scene->sun.direction = normalize(vec3(0.5, -1, 0.1));

	const float PI = 3.1415;
	float azimuth = PI/6;
	float altidude = PI/6;
	scene->sun.direction = normalize(-vec3(cos(azimuth) * sin(altidude), cos(altidude),sin(azimuth) * sin(altidude)));


	Model* soldier = global_models.find_or_load("soldier/soldier.dae");
	//load_model_assimp(soldier, "soldier/soldier.dae", false);
	scene->objects.push_back(new GameObject(soldier));
	scene->objects.back()->scale = vec3(0.03);
	scene->objects.back()->position = vec3(1, 0, 0);

//	make_qobj_from_assimp("sponza/sponza.obj", "sponza", true);
	//Model* sponza = global_models.find_or_load("sponza/sponza.obj");
	//load_model_assimp(sponza, "sponza/sponza.obj", true);
	//load_model_qobj(sponza, "sponza.qobj");
	//
	//scene->objects.push_back(new GameObject(sponza));
	//scene->objects.back()->scale = vec3(0.01);
	
	Model* revant = global_models.find_or_load("Revenant/revenant.dae");
	//load_model_assimp(revant, "Revenant/revenant.dae",false);
	scene->objects.push_back(new GameObject(revant));
	scene->objects.back()->scale = vec3(0.015);
	scene->objects.back()->position = vec3(2, 0, 1);
	scene->objects.back()->euler_x = - 3.14 / 2;

	Model* geo = global_models.find_or_load("TUNNELS.obj");
	scene->objects.push_back(new GameObject(geo));
	scene->objects.back()->scale = vec3(0.2);

	scene->num_lights = 5;
	scene->lights[0].color = 2.f*vec3(1, 0, 0);
	scene->lights[0].position = vec3(1, 1, 0);

	scene->lights[1].color = 2.f*vec3(0, 1, 0);
	scene->lights[1].position = vec3(0, 1, 1);

	scene->lights[2].color = 3.f*vec3(5, 2, 1);
	scene->lights[2].position = vec3(-2, 2, 2);

	scene->lights[3].color = 4.f*vec3(1, 1, 1);
	scene->lights[3].position = vec3(-1, 3, -1);

	scene->lights[4].color = 50.f*vec3(0, 0.4, 2);
	scene->lights[4].position = vec3(-1, 3, -1);

	u32 end = SDL_GetTicks();

	printf("\n\n\n     TIME: %u\n\n\n", end - start);

}

void App::init_window()
{
	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		sdldie("SDL init failed");
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	window = SDL_CreateWindow("3dEngine",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (window == nullptr) {
		sdldie("Failed to create window");
	}

	//SDL_SetRelativeMouseMode(SDL_TRUE);

	context = SDL_GL_CreateContext(window);
	printf("OpenGL loaded\n");
	gladLoadGLLoader(SDL_GL_GetProcAddress);
	printf("Vendor: %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf("Version: %s\n", glGetString(GL_VERSION));

	SDL_GL_SetSwapInterval(0);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glViewport(0, 0, width, height);
	glClearColor(0.5f, 0.3f, 0.2f, 1.f);

	glEnable(GL_MULTISAMPLE);

	glDepthFunc(GL_LEQUAL);

	imgui_context = ImGui::CreateContext();
	ImGui::SetCurrentContext(imgui_context);
	ImGui_ImplSDL2_InitForOpenGL(window, &context);
	ImGui_ImplOpenGL3_Init();
}
void App::handle_event(SDL_Event& event)
{
	ImGui_ImplSDL2_ProcessEvent(&event);

	switch (event.type)
	{
	case SDL_QUIT:
		running = false;
		break;

	case SDL_WINDOWEVENT:
		switch (event.window.event)
		{
		case SDL_WINDOWEVENT_RESIZED:
			glViewport(0, 0, event.window.data1, event.window.data2);
			width = event.window.data1;
			height = event.window.data2;
			update_projection_matrix();

			r->on_resize();
			break;
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEMOTION:
		if (ImGui::GetIO().WantCaptureMouse)
			return;
		break;
	case SDL_KEYDOWN:
		if (ImGui::GetIO().WantCaptureKeyboard)
			return;
		break;

	}

	editor->handle_event(event);

}
void App::quit()
{
	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
void App::update_loop()
{
	global_textures.update();

	float sec_elap = SDL_GetTicks() / 1000.f;

	editor->on_update();

	scene->lights[0].position.x = 5 * cos(sec_elap / 2.f) * cos(sec_elap / 5.f);
	scene->lights[0].position.z = 9 * sin(sec_elap);

	scene->lights[1].position.x = 8 * cos(sec_elap)*sin(sec_elap/2.f);
	scene->lights[1].position.z = 8 * sin(SDL_GetTicks() / 3000.f);

	scene->lights[3].position.x = 5 * cos(sec_elap);
	scene->lights[3].position.z = 5 * sin(sec_elap*1.5);


	scene->lights[4].position.x = 3 * sin(sec_elap / 2) * sin(sec_elap * 2);
	scene->lights[4].position.z = 6 * cos(sec_elap / 5);

	const float PI = 3.1415;
	float azimuth = SDL_GetTicks() / 5000.f;
	float altidude = PI / 3;
	scene->sun.direction = normalize(-vec3(cos(azimuth) * sin(altidude), cos(altidude), sin(azimuth) * sin(altidude)));
}
void App::on_render()
{
	r->render_scene(*scene);
	editor->on_render();
}
void App::update_projection_matrix()
{
	projection_matrix = glm::perspective(glm::radians(scene->active_camera()->fov), (float)width / (float)height, 0.1f, 100.0f);
}