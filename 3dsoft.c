// 3dsoft.c - Complete retro CGI renderer with interactive 3D view
// Compile: cl 3dsoft.c /Fe:3dsoft.exe /link opengl32.lib glu32.lib user32.lib gdi32.lib kernel32.lib /SUBSYSTEM:WINDOWS
// Or with gcc: gcc -o 3dsoft.exe 3dsoft.c -lopengl32 -lglu32 -lm

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ============================================================================
// MATH UTILITIES
// ============================================================================
typedef struct { float x, y, z; } vec3;

vec3 vec3_add(vec3 a, vec3 b) { return (vec3){a.x+b.x, a.y+b.y, a.z+b.z}; }
vec3 vec3_sub(vec3 a, vec3 b) { return (vec3){a.x-b.x, a.y-b.y, a.z-b.z}; }
vec3 vec3_mul(vec3 v, float s) { return (vec3){v.x*s, v.y*s, v.z*s}; }
float vec3_dot(vec3 a, vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
vec3 vec3_normalize(vec3 v) { float len = sqrt(vec3_dot(v,v)); return vec3_mul(v, 1.0f/len); }
vec3 vec3_cross(vec3 a, vec3 b) { return (vec3){a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }

// ============================================================================
// GLOBALS
// ============================================================================
HWND g_hwnd;
HDC g_hdc;
HGLRC g_hrc;
int g_width = 1024;
int g_height = 768;
float g_rotation = 0.0f;
float g_lightAngle = 0.0f;
float g_camX = 0, g_camY = 1.5f, g_camZ = 6.0f;
float g_camAngleX = 0, g_camAngleY = 0.3f;
int g_keys[256] = {0};
int g_mouseX = g_width/2, g_mouseY = g_height/2;
int g_firstMouse = 1;
float g_time = 0;

// Starfield data
typedef struct { float x, y, z; float brightness; } Star;
Star g_stars[1500];
int g_starCount = 1500;

// Particle data
typedef struct { float x, y, z; float vx, vy, vz; } Particle;
Particle g_particles[300];
int g_particleCount = 300;

// ============================================================================
// INITIALIZATION
// ============================================================================
void initStars() {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < g_starCount; i++) {
        g_stars[i].x = (rand() % 2000 - 1000) / 10.0f;
        g_stars[i].y = (rand() % 1000 - 500) / 10.0f;
        g_stars[i].z = -40.0f - (rand() % 300) / 10.0f;
        g_stars[i].brightness = 0.3f + (rand() % 70) / 100.0f;
    }
}

void initParticles() {
    for (int i = 0; i < g_particleCount; i++) {
        g_particles[i].x = (rand() % 600 - 300) / 50.0f;
        g_particles[i].y = (rand() % 400) / 50.0f;
        g_particles[i].z = (rand() % 600 - 300) / 50.0f;
        g_particles[i].vx = (rand() % 100 - 50) / 500.0f;
        g_particles[i].vy = (rand() % 100) / 500.0f;
        g_particles[i].vz = (rand() % 100 - 50) / 500.0f;
    }
}

// ============================================================================
// DRAWING FUNCTIONS
// ============================================================================
void drawCheckerFloor() {
    glBegin(GL_QUADS);
    for (int i = -12; i < 12; i++) {
        for (int j = -12; j < 12; j++) {
            if ((i + j) % 2 == 0)
                glColor3f(0.92f, 0.88f, 0.78f);
            else
                glColor3f(0.28f, 0.22f, 0.42f);
            
            glVertex3f(i, -0.2f, j);
            glVertex3f(i+1, -0.2f, j);
            glVertex3f(i+1, -0.2f, j+1);
            glVertex3f(i, -0.2f, j+1);
        }
    }
    glEnd();
    
    // Add subtle grid lines
    glColor3f(0.5f, 0.4f, 0.6f);
    glBegin(GL_LINES);
    for (int i = -12; i <= 12; i++) {
        glVertex3f(i, -0.19f, -12);
        glVertex3f(i, -0.19f, 12);
        glVertex3f(-12, -0.19f, i);
        glVertex3f(12, -0.19f, i);
    }
    glEnd();
}

void drawShinySphere(float x, float y, float z, float radius) {
    const int slices = 80;
    const int stacks = 80;
    
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
            
            vec3 norm1 = vec3_normalize((vec3){x1, y1, z1});
            vec3 norm2 = vec3_normalize((vec3){x2, y2, z2});
            
            vec3 lightDir = vec3_normalize((vec3){5 * sin(g_lightAngle), 8 + sin(g_lightAngle)*2, 5 * cos(g_lightAngle)});
            float diff1 = fmax(0.15f, vec3_dot(norm1, lightDir));
            float diff2 = fmax(0.15f, vec3_dot(norm2, lightDir));
            
            // Shiny metallic with rim lighting
            float shine = 0.85f + 0.15f * sin(g_rotation * 3);
            float spec1 = pow(fmax(0, vec3_dot(norm1, lightDir)), 32) * 0.8f;
            float spec2 = pow(fmax(0, vec3_dot(norm2, lightDir)), 32) * 0.8f;
            
            float r1c = 0.88f * diff1 + spec1;
            float g1c = 0.85f * diff1 + spec1 * 0.9f;
            float b1c = 0.82f * diff1 + spec1 * 0.7f;
            
            float r2c = 0.88f * diff2 + spec2;
            float g2c = 0.85f * diff2 + spec2 * 0.9f;
            float b2c = 0.82f * diff2 + spec2 * 0.7f;
            
            glColor3f(r1c * shine, g1c * shine, b1c * shine);
            glVertex3f(x + x1, y + y1, z + z1);
            
            glColor3f(r2c * shine, g2c * shine, b2c * shine);
            glVertex3f(x + x2, y + y2, z + z2);
        }
        glEnd();
    }
}

void drawStars() {
    glDisable(GL_DEPTH_TEST);
    glPointSize(1.2f);
    glBegin(GL_POINTS);
    for (int i = 0; i < g_starCount; i++) {
        float twinkle = 0.5f + 0.5f * sin(g_time * 3 + i);
        glColor3f(g_stars[i].brightness * twinkle, 
                  g_stars[i].brightness * twinkle * 0.85f, 
                  g_stars[i].brightness * twinkle);
        glVertex3f(g_stars[i].x, g_stars[i].y, g_stars[i].z);
    }
    glEnd();
    glEnable(GL_DEPTH_TEST);
}

void drawFloatingParticles() {
    glDisable(GL_LIGHTING);
    glPointSize(2.5f);
    glBegin(GL_POINTS);
    for (int i = 0; i < g_particleCount; i++) {
        // Update particle positions
        g_particles[i].x += g_particles[i].vx;
        g_particles[i].y += g_particles[i].vy;
        g_particles[i].z += g_particles[i].vz;
        
        // Wrap around
        if (g_particles[i].x > 8) g_particles[i].x = -8;
        if (g_particles[i].x < -8) g_particles[i].x = 8;
        if (g_particles[i].y > 5) g_particles[i].y = 0;
        if (g_particles[i].y < 0) g_particles[i].y = 5;
        if (g_particles[i].z > 8) g_particles[i].z = -8;
        if (g_particles[i].z < -8) g_particles[i].z = 8;
        
        float brightness = 0.4f + 0.6f * sin(g_time * 5 + i);
        glColor3f(1.0f, 0.6f * brightness, 0.2f * brightness);
        glVertex3f(g_particles[i].x, g_particles[i].y, g_particles[i].z);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

// ============================================================================
// INPUT HANDLING
// ============================================================================
void handleInput() {
    float speed = 0.08f;
    if (g_keys['W'] || g_keys['w']) {
        g_camX -= sin(g_camAngleX) * speed;
        g_camZ -= cos(g_camAngleX) * speed;
    }
    if (g_keys['S'] || g_keys['s']) {
        g_camX += sin(g_camAngleX) * speed;
        g_camZ += cos(g_camAngleX) * speed;
    }
    if (g_keys['A'] || g_keys['a']) {
        g_camX -= cos(g_camAngleX) * speed;
        g_camZ += sin(g_camAngleX) * speed;
    }
    if (g_keys['D'] || g_keys['d']) {
        g_camX += cos(g_camAngleX) * speed;
        g_camZ -= sin(g_camAngleX) * speed;
    }
    if (g_keys[VK_SPACE] && g_camY < 3) g_camY += speed;
    if (g_keys[VK_SHIFT] && g_camY > 0.5) g_camY -= speed;
    if (g_keys['R'] || g_keys['r']) {
        g_camX = 0; g_camY = 1.5f; g_camZ = 6.0f;
        g_camAngleX = 0; g_camAngleY = 0.3f;
    }
}

// ============================================================================
// RENDERING
// ============================================================================
void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // Camera setup
    gluLookAt(g_camX, g_camY, g_camZ,
              g_camX + sin(g_camAngleX), g_camY + sin(g_camAngleY), g_camZ + cos(g_camAngleX),
              0, 1, 0);
    
    // Animated light
    g_lightAngle += 0.015f;
    GLfloat lightPos[] = {5.0f * sin(g_lightAngle), 7.0f + sin(g_lightAngle*1.5f)*2, 5.0f * cos(g_lightAngle), 1.0f};
    GLfloat lightColor[] = {1.0f, 0.92f, 0.85f, 1.0f};
    GLfloat lightAmbient[] = {0.2f, 0.18f, 0.25f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    
    // Draw everything
    drawStars();
    drawFloatingParticles();
    drawShinySphere(0, 1.2f, 0, 1.25f);
    drawCheckerFloor();
    
    g_rotation += 0.02f;
    g_time += 0.016f;
    
    SwapBuffers(g_hdc);
}

// ============================================================================
// OPENGL SETUP
// ============================================================================
void initGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glClearColor(0.02f, 0.02f, 0.08f, 1.0f);
    glShadeModel(GL_SMOOTH);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(55.0, (double)g_width/g_height, 0.1, 150.0);
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================================
// WINDOW PROCEDURE
// ============================================================================
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
                int y = HIWORD(lParam);
                if (g_firstMouse) {
                    g_mouseX = x; g_mouseY = y;
                    g_firstMouse = 0;
                }
                int dx = x - g_mouseX;
                int dy = y - g_mouseY;
                g_camAngleX += dx * 0.005f;
                g_camAngleY -= dy * 0.003f;
                if (g_camAngleY > 1.4f) g_camAngleY = 1.4f;
                if (g_camAngleY < -0.8f) g_camAngleY = -0.8f;
                g_mouseX = x; g_mouseY = y;
                
                RECT rect;
                GetClientRect(hwnd, &rect);
                if (x <= 10 || x >= rect.right-10 || y <= 10 || y >= rect.bottom-10) {
                    SetCursorPos(rect.right/2, rect.bottom/2);
                    g_mouseX = rect.right/2;
                    g_mouseY = rect.bottom/2;
                }
            }
            return 0;
        case WM_SIZE:
            g_width = LOWORD(lParam);
            g_height = HIWORD(lParam);
            glViewport(0, 0, g_width, g_height);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            gluPerspective(55.0, (double)g_width/g_height, 0.1, 150.0);
            glMatrixMode(GL_MODELVIEW);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RetroCGI3DSoft";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClass(&wc);
    
    // Create window
    g_hwnd = CreateWindowEx(0, "RetroCGI3DSoft", 
                            "3DSOFT.EXE - Retro CGI Renderer | Shiny Sphere | Checker Floor | Starry Sky",
                            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
                            100, 100, g_width, g_height,
                            NULL, NULL, hInstance, NULL);
    
    if (!g_hwnd) return 1;
    
    // Setup OpenGL
    g_hdc = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 16, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
    };
    int pf = ChoosePixelFormat(g_hdc, &pfd);
    SetPixelFormat(g_hdc, pf, &pfd);
    g_hrc = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, g_hrc);
    
    // Initialize scene
    initGL();
    initStars();
    initParticles();
    
    // Hide cursor and center it
    SetCursorPos(g_width/2, g_height/2);
    ShowCursor(FALSE);
    
    // Print instructions to debug output
    OutputDebugStringA("\n========================================\n");
    OutputDebugStringA("  3DSOFT.EXE - Retro CGI Renderer\n");
    OutputDebugStringA("========================================\n");
    OutputDebugStringA("  Controls:\n");
    OutputDebugStringA("    WASD     - Move around\n");
    OutputDebugStringA("    Mouse    - Look around\n");
    OutputDebugStringA("    SPACE    - Fly up\n");
    OutputDebugStringA("    SHIFT    - Fly down\n");
    OutputDebugStringA("    R        - Reset camera\n");
    OutputDebugStringA("    ESC      - Exit\n");
    OutputDebugStringA("========================================\n\n");
    
    // Message loop
    MSG msg;
    while (1) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                wglMakeCurrent(NULL, NULL);
                wglDeleteContext(g_hrc);
                ReleaseDC(g_hwnd, g_hdc);
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        handleInput();
        renderScene();
        Sleep(16); // ~60 FPS
    }
    
    return 0;
}
