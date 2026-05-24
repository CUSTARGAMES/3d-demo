#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include "raycast.h"

int main(int argc, char* argv[]) {
    printf("Starting DOOM-Style Raycaster...\n");
    
    GameState game;
    if (!init_game(&game)) {
        printf("Failed to initialize game!\n");
        return 1;
    }
    
    printf("Controls: WASD to move, Arrow Keys to look, ESC to quit\n");
    printf("Raycaster is running!\n");
    
    game_loop(&game);
    cleanup_game(&game);
    
    return 0;
}

int main(int argc, char* argv[]) {
    printf("Starting DOOM-Style Raycaster...\n");
    
    GameState game;
    if (!init_game(&game)) {
        printf("Failed to initialize game!\n");
        return 1;
    }
    
    printf("Controls: WASD to move, Mouse to look, ESC to quit\n");
    printf("Raycaster is running!\n");
    
    game_loop(&game);
    cleanup_game(&game);
    
    return 0;
}
