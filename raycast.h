#ifndef RAYCAST_H
#define RAYCAST_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768
#define MAP_WIDTH 10
#define MAP_HEIGHT 10
#define FOV 66.0
#define MOVE_SPEED 5.0
#define ROT_SPEED 3.0

typedef struct {
    double x, y;          // Player position
    double dir_x, dir_y;  // Direction vector
    double plane_x, plane_y; // Camera plane
} Player;

typedef struct {
    int map[MAP_WIDTH][MAP_HEIGHT];
    SDL_Window* window;
    SDL_Renderer* renderer;
    Player player;
    bool running;
    Uint32 last_time;
} GameState;

bool init_game(GameState* game);
void game_loop(GameState* game);
void cleanup_game(GameState* game);
void handle_input(GameState* game);
void render(GameState* game);
void load_map(GameState* game);

#endif
