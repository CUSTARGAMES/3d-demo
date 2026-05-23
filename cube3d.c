// cube3d.c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 40
#define DISTANCE 5.0
#define CUBE_SIZE 1.0

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    Vec3 vertices[8];
    int edges[12][2];
} Cube;

// Cube vertices
const Vec3 cubeVertices[] = {
    {-1, -1, -1}, { 1, -1, -1}, { 1, -1,  1}, {-1, -1,  1}, // bottom face
    {-1,  1, -1}, { 1,  1, -1}, { 1,  1,  1}, {-1,  1,  1}  // top face
};

// Cube edges (connecting vertices)
const int cubeEdges[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0}, // bottom face
    {4,5}, {5,6}, {6,7}, {7,4}, // top face
    {0,4}, {1,5}, {2,6}, {3,7}  // vertical edges
};

// Rotation angles
float angleX = 0, angleY = 0;

// Function to rotate a point around X and Y axes
Vec3 rotatePoint(Vec3 p, float ax, float ay) {
    Vec3 result;
    float cosX = cos(ax), sinX = sin(ax);
    float cosY = cos(ay), sinY = sin(ay);
    
    // Rotate around X axis
    float y1 = p.y * cosX - p.z * sinX;
    float z1 = p.y * sinX + p.z * cosX;
    
    // Rotate around Y axis
    result.x = p.x * cosY + z1 * sinY;
    result.z = -p.x * sinY + z1 * cosY;
    result.y = y1;
    
    return result;
}

// Project 3D point to 2D screen coordinates
void projectPoint(Vec3 p, int *screenX, int *screenY, int *isVisible) {
    // Perspective projection
    float z = p.z + DISTANCE;
    if (z > 0.1) {
        float factor = DISTANCE / z;
        *screenX = (int)(SCREEN_WIDTH / 2 + p.x * factor * SCREEN_WIDTH / 4);
        *screenY = (int)(SCREEN_HEIGHT / 2 - p.y * factor * SCREEN_HEIGHT / 4);
        *isVisible = 1;
    } else {
        *isVisible = 0;
    }
}

// Draw line using Bresenham's algorithm
void drawLine(int x1, int y1, int x2, int y2, char screen[SCREEN_HEIGHT][SCREEN_WIDTH]) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int x = x1, y = y1;
    
    while (1) {
        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
            screen[y][x] = '#';
        }
        if (x == x2 && y == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

// Clear screen
void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[1;1H");
#endif
}

// Get keyboard input (non-blocking)
int getKeyPress() {
#ifdef _WIN32
    if (_kbhit()) {
        return _getch();
    }
    return 0;
#else
    struct termios oldt, newt;
    int ch;
    int oldf;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    if (ch != EOF) {
        return ch;
    }
    return 0;
#endif
}

// Main function
int main() {
    Cube cube;
    int running = 1;
    int key;
    
    // Initialize cube
    for (int i = 0; i < 8; i++) {
        cube.vertices[i] = cubeVertices[i];
    }
    for (int i = 0; i < 12; i++) {
        cube.edges[i][0] = cubeEdges[i][0];
        cube.edges[i][1] = cubeEdges[i][1];
    }
    
    printf("3D Cube Rotation Demo\n");
    printf("Use Arrow Keys to Rotate\n");
    printf("Press ESC or Q to quit\n");
    Sleep(2000);
    
    while (running) {
        // Check keyboard input
        key = getKeyPress();
        switch (key) {
            case 72: // Up arrow
            case 'w':
            case 'W':
                angleX += 0.05;
                break;
            case 80: // Down arrow
            case 's':
            case 'S':
                angleX -= 0.05;
                break;
            case 75: // Left arrow
            case 'a':
            case 'A':
                angleY -= 0.05;
                break;
            case 77: // Right arrow
            case 'd':
            case 'D':
                angleY += 0.05;
                break;
            case 27: // ESC
            case 'q':
            case 'Q':
                running = 0;
                break;
        }
        
        // Create screen buffer
        char screen[SCREEN_HEIGHT][SCREEN_WIDTH];
        for (int i = 0; i < SCREEN_HEIGHT; i++) {
            for (int j = 0; j < SCREEN_WIDTH; j++) {
                screen[i][j] = ' ';
            }
        }
        
        // Draw edges
        for (int i = 0; i < 12; i++) {
            // Get rotated vertices
            Vec3 p1 = rotatePoint(cube.vertices[cube.edges[i][0]], angleX, angleY);
            Vec3 p2 = rotatePoint(cube.vertices[cube.edges[i][1]], angleX, angleY);
            
            int x1, y1, x2, y2;
            int v1, v2;
            
            projectPoint(p1, &x1, &y1, &v1);
            projectPoint(p2, &x2, &y2, &v2);
            
            if (v1 && v2) {
                drawLine(x1, y1, x2, y2, screen);
            }
        }
        
        // Render screen
        clearScreen();
        for (int i = 0; i < SCREEN_HEIGHT; i++) {
            for (int j = 0; j < SCREEN_WIDTH; j++) {
                putchar(screen[i][j]);
            }
            putchar('\n');
        }
        
        // Add frame counter and instructions
        printf("Angles: X=%.2f Y=%.2f | Use Arrows/WASD to rotate | Press Q to quit\n", 
               angleX, angleY);
        
        Sleep(50); // ~20 FPS
    }
    
    clearScreen();
    printf("Goodbye!\n");
    return 0;
}
