#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include "glm/gtc/matrix_transform.hpp"

#include "camera.h"
#include "loader.h"

#include "opengl_api.h"

#include "types.h"
#include "app.h"
#include "renderer.h"


using namespace std;


/* Virtual Interface for Game Logic */
class IGamee
{
	IGamee();
	virtual ~IGamee() = 0;
};


int main(int argc, char** argv)
{
	App& app = global_app;
	app.init();

	while (app.running)
	{
		app.delta_t = SDL_GetTicks() - app.last_t;
		app.last_t = SDL_GetTicks();
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			app.handle_event(event);
		}
		app.update_loop();
		app.on_render();

		SDL_GL_SwapWindow(app.window);
	}

	app.quit();
	return 0;
}
