#ifndef EDITOR
#define EDITOR
#include <SDL2/SDL.h>

class App;
class Editor
{
public:
	Editor(App& app);
	void handle_event(SDL_Event& event);
	void on_render();
	void on_update();
	

private:

	bool game_focused = true;
	int cam_num = 0;

	bool show_light_lines = true;
	bool show_shadow_map = false;

	//Bloom viwer

	App& app;
};
#endif // !EDITOR