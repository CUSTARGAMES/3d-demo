// render.c - Pure C, compiles with gcc, no external deps!
// Compile: gcc -o render render.c -lm -O3 -Wall -mwindows
// Run: ./render.exe

#include <windows.h>
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Simple vector math
typedef struct { float x, y, z; } vec3;

vec3 vec3_add(vec3 a, vec3 b) { return (vec3){a.x+b.x, a.y+b.y, a.z+b.z}; }
vec3 vec3_sub(vec3 a, vec3 b) { return (vec3){a.x-b.x, a.y-b.y, a.z-b.z}; }
vec3 vec3_mul(vec3 v, float s) { return (vec3){v.x*s, v.y*s, v.z*s}; }
float vec3_dot(vec3 a, vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
vec3 vec3_normalize(vec3 v) { float len = sqrt(vec3_dot(v,v)); return vec3_mul(v, 1.0f/len); }

// Global variables for animation
float g_rotation = 0.0f;
float g_lightAngle = 0.0f;
int g_width = 1024;
int g_height = 768;
float g_camX = 0, g_camY = 1.5f, g_camZ = 6.0f;
float g_camAngle = 0;
int g_keys[256] = {0};

// Draw checkerboard floor
void drawFloor() {
    glBegin(GL_QUADS);
    for (int i = -8; i < 8; i++) {
        for (int j = -8; j < 8; j++) {
            // Alternate colors based on position
            if ((i + j) % 2 == 0)
                glColor3f(0.9f, 0.85f, 0.75f);  // light tile
            else
                glColor3f(0.35f, 0.25f, 0.45f); // dark purple-grey
            
            glVertex3f(i, -0.2f, j);
            glVertex3f(i+1, -0.2f, j);
            glVertex3f(i+1, -0.2f, j+1);
            glVertex3f(i, -0.2f, j+1);
        }
    }
    glEnd();
}

// Draw shiny sphere with reflection-like shading
void drawSphere(float x, float y, float z, float radius) {
    const int slices = 64;
    const int stacks = 64;
    
    for (int i = 0; i < stacks; i++) {
        float phi1 = M_PI * i / stacks;
        float phi2 = M_PI * (i+1) / stacks;
        float y1 = radius * cos(phi1);
        float y2 = radius * cos(phi2);
        float r1 = radius * sin(phi1);
        float r2 = radius * sin(phi2);
        
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float theta = 2 * M_PI * j / slices;
            float x1 = r1 * cos(theta);
            float z1 = r1 * sin(theta);
            float x2 = r2 * cos(theta);
            float z2 = r2 * sin(theta);
            
            // Calculate normal for lighting
            vec3 norm1 = vec3_normalize((vec3){x1, y1, z1});
            vec3 norm2 = vec3_normalize((vec3){x2, y2, z2});
            
            // Simple lighting calculation
            vec3 lightDir = vec3_normalize((vec3){5, 10, 5});
            float diff1 = fmax(0.2f, vec3_dot(norm1, lightDir));
            float diff2 = fmax(0.2f, vec3_dot(norm2, lightDir));
            
            // Shiny metallic color
            float shine = 0.7f + 0.3f * sin(g_rotation * 2);
            glColor3f(0.85f * diff1 * shine, 0.82f * diff1 * shine, 0.78f * diff1 * shine);
            glVertex3f(x + x1, y + y1, z + z1);
            
            glColor3f(0.85f * diff2 * shine, 0.82f * diff2 * shine, 0.78f * diff2 * shine);
            glVertex3f(x + x2, y + y2, z + z2);
        }
        glEnd();
    }
}

// Draw stars in background
void drawStars() {
    static int starCount = 800;
    static float stars[800][3];
    static int initialized = 0;
    
    if (!initialized) {
        srand(time(NULL));
        for (int i = 0; i < starCount; i++) {
            stars[i][0] = (rand() % 2000 - 1000) / 10.0f;
            stars[i][1] = (rand() % 1000 - 500) / 10.0f;
            stars[i][2] = -30.0f - (rand() % 200) / 10.0f;
        }
        initialized = 1;
    }
    
    glPointSize(1.5f);
    glBegin(GL_POINTS);
    glColor3f(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < starCount; i++) {
        // Twinkle effect
        float brightness = 0.5f + 0.5f * sin(g_rotation * 5 + i);
        glColor3f(brightness, brightness * 0.9f, brightness);
        glVertex3f(stars[i][0], stars[i][1], stars[i][2]);
    }
    glEnd();
}

// Handle keyboard input
void handleInput() {
    float speed = 0.1f;
    if (g_keys['W'] || g_keys['w']) {
        g_camX -= sin(g_camAngle) * speed;
        g_camZ -= cos(g_camAngle) * speed;
    }
    if (g_keys['S'] || g_keys['s']) {
        g_camX += sin(g_camAngle) * speed;
        g_camZ += cos(g_camAngle) * speed;
    }
    if (g_keys['A'] || g_keys['a']) {
        g_camX -= cos(g_camAngle) * speed;
        g_camZ += sin(g_camAngle) * speed;
    }
    if (g_keys['D'] || g_keys['d']) {
        g_camX += cos(g_camAngle) * speed;
        g_camZ -= sin(g_camAngle) * speed;
    }
    if (g_keys[VK_SPACE]) g_camY += speed;
    if (g_keys[VK_SHIFT]) g_camY -= speed;
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            g_keys[wParam] = 1;
            if (wParam == VK_ESCAPE) PostQuitMessage(0);
            return 0;
        case WM_KEYUP:
            g_keys[wParam] = 0;
            return 0;
        case WM_MOUSEMOVE:
            {
                int x = LOWORD(lParam);
                int dx = x - g_width/2;
                g_camAngle += dx * 0.005f;
                SetCursorPos(g_width/2, g_height/2);
            }
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Setup OpenGL
void initGL(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 16, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
    };
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    wglMakeCurrent(hdc, wglCreateContext(hdc));
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    
    GLfloat lightPos[] = {5.0f, 8.0f, 5.0f, 1.0f};
    GLfloat lightAmbient[] = {0.2f, 0.2f, 0.2f, 1.0f};
    GLfloat lightDiffuse[] = {1.0f, 0.95f, 0.9f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
}

// Main render loop
void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // Camera setup
    gluLookAt(g_camX, g_camY, g_camZ,
              g_camX + sin(g_camAngle), g_camY, g_camZ + cos(g_camAngle),
              0, 1, 0);
    
    // Update animated light position
    g_lightAngle += 0.02f;
    GLfloat lightPos[] = {5.0f * sin(g_lightAngle), 7.0f + sin(g_lightAngle)*2, 5.0f * cos(g_lightAngle), 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    
    // Draw stars (no depth test for background)
    glDisable(GL_DEPTH_TEST);
    drawStars();
    glEnable(GL_DEPTH_TEST);
    
    // Draw shiny sphere
    drawSphere(0, 1.2f, 0, 1.2f);
    
    // Draw checkerboard floor
    drawFloor();
    
    // Add some floating particles for retro effect
    glDisable(GL_LIGHTING);
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 200; i++) {
        float angle = g_rotation * 2 + i;
        float x = sin(angle) * 2.5f;
        float z = cos(angle * 0.7f) * 2.5f;
        float y = 1.0f + sin(angle * 1.3f) * 1.2f;
        glColor3f(1.0f, 0.7f, 0.3f);
        glVertex3f(x, y, z);
    }
    glEnd();
    glEnable(GL_LIGHTING);
    
    g_rotation += 0.02f;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RetroCGI";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    
    // Create window
    HWND hwnd = CreateWindowEx(0, "RetroCGI", "Retro CGI - Shiny Sphere | Checker Floor | Starry Sky",
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, g_width, g_height,
                               NULL, NULL, hInstance, NULL);
    
    // Center mouse
    SetCursorPos(g_width/2, g_height/2);
    ShowCursor(FALSE);
    
    // Setup OpenGL
    initGL(hwnd);
    glViewport(0, 0, g_width, g_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)g_width/g_height, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
    
    // Main loop
    MSG msg;
    while (1) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (msg.message == WM_QUIT) break;
        
        handleInput();
        renderScene();
        SwapBuffers(GetDC(hwnd));
        Sleep(16); // ~60 FPS
    }
    
    return 0;
}
