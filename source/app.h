#ifndef EDITOR_H
#define EDITOR_H

#include <SDL2/SDL.h>
#include "glm/glm.hpp"
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

	glm::mat4 projection_matrix;

	App();
	void init();
	void handle_event(SDL_Event& event);

	void update_loop();
	void on_render();
	void quit();

	void create_scene();

	void update_projection_matrix();

	// Bad??
	static const App& get() {
		return *global_app;
	}
	static void set_global(App* a) {
		global_app = a;
	}

	void Printf();
	void Warning();
	void Error();

	Renderer* r;
	SceneData* scene;
	Editor* editor;

private:
	static App* global_app;
};

//extern App global_app;
#endif // !EDITOR_H