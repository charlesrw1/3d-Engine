#include "app.h"
#include "renderer.h"
#include "types.h"
#include <SDL2/SDL.h>
#include "glad/glad.h"
#include "glm/gtc/matrix_transform.hpp"


#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"

#include "editor.h"

#include "ModelManager.h"

#include "TextureManager.h"
#include "Texture_def.h"

#include "MapParser.h"

#include "WorldGeometry.h"
#include "Light.h"

#include <fstream>

App global_app;

void App::init()
{
	init_window();

	r = new Renderer;
	scene = new SceneData;
	editor = new Editor;

	global_world.set_worldmodel(&world);
	global_world.init_render_data();

	//create_scene();
	r->set_fov(45);
	r->resize(width, height);
}

void sdldie(const char* msg)
{
	printf(" % s: % s\n", msg, SDL_GetError());
	SDL_Quit();
	exit(1);
}

vec3 make_color(uint8_t r, uint8_t b, uint8_t g) {
	return vec3(r / 255.f, b / 255.f, g / 255.f);
}

const entity_t* find_entity_with_classname(const worldmodel_t* wm, const char* classname) {
	for (const auto& e : wm->entities) {
		auto classname_prop = e.properties.find("classname");
		if (classname_prop == e.properties.end())
			continue;
		if (classname_prop->second == classname) {
			return &e;
		}
	}
	return nullptr;
}

void App::create_scene()
{
	
	MapParser mp(false);
	u32 map_start = SDL_GetTicks();
	load_compiled_map("pillars.cmf");
	
	//mp.start_file("resources/maps/pillars.map");

	//mp.construct_mesh(scene->map_geo, scene->map_geo_edges);
	//mp.move_to_worldmodel(&world);

	global_world.load_map(&world);
	//create_light_map(&world);

	r->lightmap_tex_nearest = global_textures.find_or_load((world.name + "_lm.bmp").c_str(),NEAREST);
	Texture* l_linear = new Texture;
	l_linear->init_from_file(world.name+ "_lm.bmp", LOAD_NOW);
	r->lightmap_tex_linear = l_linear;	// temporary, memory leak cause texture manager doesnt know multiple params


	//load_compiled_map("pillars.cmf");
	global_world.create_mesh();

//	write_compiled_map();



	u32 map_end = SDL_GetTicks();
	printf("Loaded map in: %i ms\n", map_end - map_start);
	//exit(1);
	//return;
	auto player_start = find_entity_with_classname(&world, "info_player_start");
	auto org = player_start->properties.find("origin");
	vec3 start = vec3(0);
	if (org != player_start->properties.end()) {
		sscanf_s(org->second.c_str(), "%f %f %f", &start.x, &start.y, &start.z);
		start = vec3(-start.x, start.z, start.y)/32.f;
	}
	scene->cams[0].position = start;
	


	u32 start_load = SDL_GetTicks();

	
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

	//GameObject* temp;
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


	//Model* soldier = global_models.find_or_load("soldier/soldier.dae");
	////load_model_assimp(soldier, "soldier/soldier.dae", false);
	//scene->objects.push_back(new GameObject(soldier));
	//scene->objects.back()->scale = vec3(0.03);
	//scene->objects.back()->position = vec3(1, 0, 0);

//	make_qobj_from_assimp("sponza/sponza.obj", "sponza", true);
	//Model* sponza = global_models.find_or_load("sponza/sponza.obj");
	//load_model_assimp(sponza, "sponza/sponza.obj", true);
	//load_model_qobj(sponza, "sponza.qobj");
	//
	//scene->objects.push_back(new GameObject(sponza));
	//scene->objects.back()->scale = vec3(0.01);
	
	//Model* revant = global_models.find_or_load("Revenant/revenant.dae");
	////load_model_assimp(revant, "Revenant/revenant.dae",false);
	//scene->objects.push_back(new GameObject(revant));
	//scene->objects.back()->scale = vec3(0.015);
	//scene->objects.back()->position = vec3(2, 0, 1);
	//scene->objects.back()->euler_x = - 3.14 / 2;

	//Model* geo = global_models.find_or_load("TUNNELS.obj");
	//scene->objects.push_back(new GameObject(geo));
	//scene->objects.back()->scale = vec3(0.2);

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

	global_textures.update();
	printf("\n\n\n     TIME: %u\n\n\n", end - start_load);

	//global_models.print_info();
	//global_textures.print_info();

}

void App::compile_map(std::string map_name, LightmapSettings& lm_s, bool quake_format)
{
	MapParser mp(quake_format);
	mp.start_file("resources/maps/" + map_name);
	mp.move_to_worldmodel(&world);
	global_world.upload_map_to_tree();

	create_light_map(&world, lm_s);

	write_compiled_map();
}
void App::setup_new_map()
{
	r->lightmap_tex_nearest = global_textures.find_or_load((world.name + "_lm.bmp").c_str(), NEAREST);
	Texture* l_linear = new Texture;
	l_linear->init_from_file(world.name + "_lm.bmp", LOAD_NOW);
	r->lightmap_tex_linear = l_linear;	// temporary, memory leak cause texture manager doesnt know multiple params


	global_world.create_mesh();
	scene->cams[0].set_angles(0, 0);
	const entity_t* player_start = find_entity_with_classname(&world, "info_player_start");
	if (!player_start) {
		scene->cams[0].position = vec3(0);
		return;
	}
	auto org = player_start->properties.find("origin");
	vec3 start = vec3(0);
	if (org != player_start->properties.end()) {
		sscanf_s(org->second.c_str(), "%f %f %f", &start.x, &start.y, &start.z);
		start = vec3(-start.x, start.z, start.y) / 32.f;
	}
	scene->cams[0].set_position(start);
}
void App::load_map(std::string map_name)
{
	world.entities.clear();
	load_compiled_map(map_name);
	// This isnt really nessecary, but its nice to demo it
	global_world.upload_map_to_tree();
}
void App::free_current_map()
{
	global_world.free_map();
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
	printf("Version: %s\n\n", glGetString(GL_VERSION));

	SDL_GL_SetSwapInterval(1);

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
			r->resize(width, height);
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

#define WRITE(ptr, count) outfile.write((char*)ptr, count)
#define WRITE_ARRAY_SIZE(size) count_temp = size;\
		WRITE(&count_temp,sizeof(int))

static const char magic[] = { 'C', 'M', 'F' };
void App::write_compiled_map()
{
	std::ofstream outfile("resources/maps/" + world.name + ".cmf", std::ios::binary);
	int count_temp;
	if (!outfile) {
		printf("Failed to write compiled map\n");
		return;
	}
	for (int i = 0; i < 3; i++) {
		WRITE(&magic[i], 1);
	}

	WRITE(world.name.c_str(), world.name.size() + 1);

	WRITE("MODELS", 7);
	WRITE_ARRAY_SIZE(world.models.size());
	for (int i = 0; i < world.models.size(); i++) {
		WRITE(&world.models[i].face_start, sizeof(int));
		WRITE(&world.models[i].face_count, sizeof(int));
		WRITE(&world.models[i].min, sizeof(vec3));
		WRITE(&world.models[i].max, sizeof(vec3));
	}

	assert(sizeof(vec3) == 12);
	WRITE("VERTS", 6);
	WRITE_ARRAY_SIZE(world.verts.size());
	WRITE(world.verts.data(), world.verts.size() * sizeof(vec3));

	assert(sizeof(plane_t) == 16);
	WRITE("FACES", 6);
	WRITE_ARRAY_SIZE(world.faces.size());
	for (int i = 0; i < world.faces.size(); i++) {
		WRITE(&world.faces[i].plane, sizeof(plane_t));
		WRITE(&world.faces[i].v_start, sizeof(int));
		WRITE(&world.faces[i].v_count, sizeof(int));
		WRITE(&world.faces[i].t_info_idx, sizeof(int));
		WRITE(&world.faces[i].lightmap_min[0], sizeof(short));
		WRITE(&world.faces[i].lightmap_min[1], sizeof(short));
		WRITE(&world.faces[i].lightmap_size[0], sizeof(short));
		WRITE(&world.faces[i].lightmap_size[1], sizeof(short));
		WRITE(&world.faces[i].lightmap_size[1], sizeof(short));
		WRITE(&world.faces[i].exact_min[0], sizeof(float));
		WRITE(&world.faces[i].exact_min[1], sizeof(float));
		WRITE(&world.faces[i].exact_span[0], sizeof(float));
		WRITE(&world.faces[i].exact_span[1], sizeof(float));
		WRITE(&world.faces[i].dont_draw, 1);
	}

	assert(sizeof(texture_info_t) == 48);
	WRITE("TINFO", 6);
	WRITE_ARRAY_SIZE(world.t_info.size());
	WRITE(world.t_info.data(), world.t_info.size() * sizeof(texture_info_t));

	WRITE("TNAME", 6);
	WRITE_ARRAY_SIZE(world.texture_names.size());
	for (int i = 0; i < world.texture_names.size(); i++) {
		WRITE(world.texture_names[i].c_str(), world.texture_names[i].size()+1);
	}

	WRITE("ENTS", 5);
	WRITE_ARRAY_SIZE(world.entities.size());
	for (int i = 0; i < world.entities.size(); i++) {
		entity_t* e = &world.entities[i];
		WRITE_ARRAY_SIZE(e->properties.size());
		for (auto prop : e->properties) {
			WRITE(prop.first.c_str(), prop.first.size() + 1);
			WRITE(prop.second.c_str(), prop.second.size() + 1);
		}
	}	
}
#define READ(ptr,count)infile.read((char*)ptr, count)

#define READ_STR(ptr) \
	temp_str_ptr = ptr; \
	while(infile.peek()!='\0') { \
		*temp_str_ptr = infile.get();	\
		temp_str_ptr++;	\
	} \
	*temp_str_ptr = infile.get(); 
void App::load_compiled_map(std::string map_name)
{
	char string_buffer[256];
	std::ifstream infile("resources/maps/" + map_name, std::ios::binary);
	if (!infile) {
		printf("Couldn't open compiled map\n");
		return;
	}
	char magic_buf[3];
	READ(&magic_buf[0], 3);
	for (int i = 0; i < 3; i++) {
		if (magic_buf[i] != magic[i]) {
			printf("Unknown file format\n");
			return;
		}
	}
	char* temp_str_ptr = nullptr;
	READ_STR(&string_buffer[0]);
	world.name = std::string(string_buffer);

	int count_temp{};

	READ_STR(&string_buffer[0]);
	READ(&count_temp, 4);
	world.models.resize(count_temp);

	for (int i = 0; i < world.models.size(); i++) {
		READ(&world.models[i].face_start, sizeof(int));
		READ(&world.models[i].face_count, sizeof(int));
		READ(&world.models[i].min, sizeof(vec3));
		READ(&world.models[i].max, sizeof(vec3));
	}

	assert(sizeof(vec3) == 12);
	READ_STR(&string_buffer[0]);
	READ(&count_temp, 4);
	world.verts.resize(count_temp);

	READ(world.verts.data(), world.verts.size() * sizeof(vec3));

	assert(sizeof(plane_t) == 16);
	READ_STR(&string_buffer[0]);
	READ(&count_temp, 4);
	world.faces.resize(count_temp);

	for (int i = 0; i < world.faces.size(); i++) {
		READ(&world.faces[i].plane, sizeof(plane_t));
		READ(&world.faces[i].v_start, sizeof(int));
		READ(&world.faces[i].v_count, sizeof(int));
		READ(&world.faces[i].t_info_idx, sizeof(int));
		READ(&world.faces[i].lightmap_min[0], sizeof(short));
		READ(&world.faces[i].lightmap_min[1], sizeof(short));
		READ(&world.faces[i].lightmap_size[0], sizeof(short));
		READ(&world.faces[i].lightmap_size[1], sizeof(short));
		READ(&world.faces[i].lightmap_size[1], sizeof(short));
		READ(&world.faces[i].exact_min[0], sizeof(float));
		READ(&world.faces[i].exact_min[1], sizeof(float));
		READ(&world.faces[i].exact_span[0], sizeof(float));
		READ(&world.faces[i].exact_span[1], sizeof(float));
		READ(&world.faces[i].dont_draw, 1);
	}
	assert(sizeof(texture_info_t) == 48);
	READ_STR(&string_buffer[0]);
	READ(&count_temp, 4);
	world.t_info.resize(count_temp);

	READ(world.t_info.data(), world.t_info.size() * sizeof(texture_info_t));

	READ_STR(&string_buffer[0]);
	READ(&count_temp, 4);
	world.texture_names.resize(count_temp);

	for (int i = 0; i < world.texture_names.size(); i++) {
		READ_STR(&string_buffer[0]);
		world.texture_names[i] = string_buffer;
	}

	READ_STR(&string_buffer[0]);
	READ(&count_temp, 4);
	world.entities.resize(count_temp);

	std::string s1, s2;
	for (int i = 0; i < world.entities.size(); i++) {
		entity_t* e = &world.entities[i];

		READ(&count_temp, 4);

		for (int j = 0; j < count_temp; j++) {
			READ_STR(&string_buffer[0]);
			s1 = string_buffer;
			READ_STR(&string_buffer[0]);
			s2 = string_buffer;
			e->properties.insert({ s1,s2 });
		}
	}

}