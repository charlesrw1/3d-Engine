#ifndef IGAME_H
#define IGAME_H

#include "SDL2/SDL_events.h"


class IGame
{
	virtual ~IGame() = 0;

	virtual void init() = 0;
	virtual void shutdown() = 0;

	virtual void run_frame() = 0;

	virtual void draw() = 0;

	virtual void handle_event(SDL_Event& event) = 0;

	virtual void init_from_map(const char* map_file) = 0;
};


#endif // !IGAME_H

