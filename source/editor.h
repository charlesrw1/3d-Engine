#ifndef EDITOR
#define EDITOR
#include <SDL2/SDL.h>
#include "WorldGeometry.h"


class Editor
{
public:
	Editor();
	void handle_event(SDL_Event& event);
	void on_render();
	void on_update();
	
private:
	void handle_scroll(SDL_Event& e);
	void shoot_ray();
	bool game_focused = true;
	int cam_num = 0;

	bool show_light_lines = true;
	bool show_shadow_map = false;

	char buffer[256];


	std::vector<trace_t> world_hits;

	//Bloom viwer
};
#endif // !EDITOR