#include "raycast.h"
#include <math.h>
#include <stdio.h>

bool init_game(GameState* game) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    
    game->window = SDL_CreateWindow("DOOM-Style Raycaster",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    
    if (!game->window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    
    game->renderer = SDL_CreateRenderer(game->window, -1, SDL_RENDERER_ACCELERATED);
    if (!game->renderer) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    
    load_map(game);
    
    game->player.x = 3.5;
    game->player.y = 3.5;
    game->player.dir_x = -1.0;
    game->player.dir_y = 0.0;
    game->player.plane_x = 0.0;
    game->player.plane_y = 0.66;
    
    game->running = true;
    game->last_time = SDL_GetTicks();
    
    return true;
}

void load_map(GameState* game) {
    // Simple maze-like map (1 = wall, 0 = empty)
    int map_data[MAP_WIDTH][MAP_HEIGHT] = {
        {1,1,1,1,1,1,1,1,1,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,1,1,0,0,1,1,0,1},
        {1,0,1,0,0,0,0,1,0,1},
        {1,0,0,0,1,1,0,0,0,1},
        {1,0,1,0,1,1,0,1,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,1,0,1,1,1,0,1,1,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,1,1,1,1,1,1,1,1,1}
    };
    
    for (int i = 0; i < MAP_WIDTH; i++) {
        for (int j = 0; j < MAP_HEIGHT; j++) {
            game->map[i][j] = map_data[i][j];
        }
    }
}

void handle_input(GameState* game) {
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    double move_x = 0, move_y = 0;
    double rot = 0;
    double frame_time = (SDL_GetTicks() - game->last_time) / 1000.0;
    game->last_time = SDL_GetTicks();
    
    if (keys[SDL_SCANCODE_W]) {
        move_x += game->player.dir_x * MOVE_SPEED * frame_time;
        move_y += game->player.dir_y * MOVE_SPEED * frame_time;
    }
    if (keys[SDL_SCANCODE_S]) {
        move_x -= game->player.dir_x * MOVE_SPEED * frame_time;
        move_y -= game->player.dir_y * MOVE_SPEED * frame_time;
    }
    if (keys[SDL_SCANCODE_A]) {
        move_x += game->player.dir_y * MOVE_SPEED * frame_time;
        move_y -= game->player.dir_x * MOVE_SPEED * frame_time;
    }
    if (keys[SDL_SCANCODE_D]) {
        move_x -= game->player.dir_y * MOVE_SPEED * frame_time;
        move_y += game->player.dir_x * MOVE_SPEED * frame_time;
    }
    if (keys[SDL_SCANCODE_LEFT]) rot = -ROT_SPEED * frame_time;
    if (keys[SDL_SCANCODE_RIGHT]) rot = ROT_SPEED * frame_time;
    if (keys[SDL_SCANCODE_ESCAPE]) game->running = false;
    
    // Collision detection
    int new_x = (int)(game->player.x + move_x);
    int new_y = (int)(game->player.y + move_y);
    
    if (game->map[new_x][(int)game->player.y] == 0)
        game->player.x += move_x;
    if (game->map[(int)game->player.x][new_y] == 0)
        game->player.y += move_y;
    
    // Rotation
    double old_dir_x = game->player.dir_x;
    game->player.dir_x = game->player.dir_x * cos(rot) - game->player.dir_y * sin(rot);
    game->player.dir_y = old_dir_x * sin(rot) + game->player.dir_y * cos(rot);
    
    double old_plane_x = game->player.plane_x;
    game->player.plane_x = game->player.plane_x * cos(rot) - game->player.plane_y * sin(rot);
    game->player.plane_y = old_plane_x * sin(rot) + game->player.plane_y * cos(rot);
}

void render(GameState* game) {
    SDL_SetRenderDrawColor(game->renderer, 0, 0, 0, 255);
    SDL_RenderClear(game->renderer);
    
    // Raycasting
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        double camera_x = 2 * x / (double)SCREEN_WIDTH - 1;
        double ray_dir_x = game->player.dir_x + game->player.plane_x * camera_x;
        double ray_dir_y = game->player.dir_y + game->player.plane_y * camera_x;
        
        int map_x = (int)game->player.x;
        int map_y = (int)game->player.y;
        
        double delta_dist_x = fabs(1 / ray_dir_x);
        double delta_dist_y = fabs(1 / ray_dir_y);
        
        int step_x, step_y;
        double side_dist_x, side_dist_y;
        
        if (ray_dir_x < 0) {
            step_x = -1;
            side_dist_x = (game->player.x - map_x) * delta_dist_x;
        } else {
            step_x = 1;
            side_dist_x = (map_x + 1.0 - game->player.x) * delta_dist_x;
        }
        
        if (ray_dir_y < 0) {
            step_y = -1;
            side_dist_y = (game->player.y - map_y) * delta_dist_y;
        } else {
            step_y = 1;
            side_dist_y = (map_y + 1.0 - game->player.y) * delta_dist_y;
        }
        
        int hit = 0;
        int side = 0;
        
        while (!hit) {
            if (side_dist_x < side_dist_y) {
                side_dist_x += delta_dist_x;
                map_x += step_x;
                side = 0;
            } else {
                side_dist_y += delta_dist_y;
                map_y += step_y;
                side = 1;
            }
            if (game->map[map_x][map_y] > 0) hit = 1;
        }
        
        double perp_wall_dist;
        if (side == 0)
            perp_wall_dist = (side_dist_x - delta_dist_x);
        else
            perp_wall_dist = (side_dist_y - delta_dist_y);
        
        int line_height = (int)(SCREEN_HEIGHT / perp_wall_dist);
        int draw_start = -line_height / 2 + SCREEN_HEIGHT / 2;
        if (draw_start < 0) draw_start = 0;
        int draw_end = line_height / 2 + SCREEN_HEIGHT / 2;
        if (draw_end >= SCREEN_HEIGHT) draw_end = SCREEN_HEIGHT - 1;
        
        // Wall color based on side
        int r, g, b;
        if (side == 0) {
            r = 255; g = 0; b = 0;  // Red for north/south walls
        } else {
            r = 0; g = 255; b = 0;  // Green for east/west walls
        }
        
        SDL_SetRenderDrawColor(game->renderer, r, g, b, 255);
        SDL_RenderDrawLine(game->renderer, x, draw_start, x, draw_end);
    }
    
    SDL_RenderPresent(game->renderer);
}

void game_loop(GameState* game) {
    SDL_Event e;
    while (game->running) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                game->running = false;
            }
        }
        
        handle_input(game);
        render(game);
        SDL_Delay(16);  // ~60 FPS
    }
}

void cleanup_game(GameState* game) {
    SDL_DestroyRenderer(game->renderer);
    SDL_DestroyWindow(game->window);
    SDL_Quit();
}
