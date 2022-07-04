#ifndef EDITOR_H
#define EDITOR_H

#include <SDL2/SDL.h>
#include "glm/glm.hpp"
#include "Map_def.h"
#include "Light.h"
class Renderer;
class Editor;
struct SceneData;
struct ImGuiContext;
class App
{
public:
	// Window data
	SDL_Window* window = nullptr;
	SDL_GLContext context;
	ImGuiContext* imgui_context;

	int width = 1600;
	int height = 900;
	float aspect_r = 1600 / 900.f;

	int delta_t;
	int last_t;

	bool running = true;

	void init();
	void init_window();

	void handle_event(SDL_Event& event);

	void update_loop();
	void on_render();
	void quit();

	void create_scene();

	// Called after either loading a compiled map or compiling a map
	void setup_new_map();
	// Loads a compiled map
	void load_map(std::string file);
	// Compiles a map and writes to disk
	void compile_map(std::string map_name, LightmapSettings& lm_s, bool quake_format);
	// Called by "load_map"
	void load_compiled_map(std::string map_file);
	void write_compiled_map();
	void free_current_map();


	Renderer* r;
	SceneData* scene;
	Editor* editor;

	worldmodel_t world;
};

extern App global_app;
#endif // !EDITOR_H