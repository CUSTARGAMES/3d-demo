// 3dsoft.c - Retro CGI Renderer with Shiny Sphere, Checker Floor & Starry Sky
// Compile: gcc -o 3dsoft.exe 3dsoft.c -lopengl32 -lglu32 -lm -mwindows

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

// Simple vector math
typedef struct { float x, y, z; } vec3;

// Global variables
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
int g_mouseX = 512, g_mouseY = 384;
int g_firstMouse = 1;
float g_time = 0;

// Star data
typedef struct { float x, y, z; float brightness; } Star;
Star g_stars[1500];
int g_starCount = 1500;

// Particle data
typedef struct { float x, y, z; float vx, vy, vz; } Particle;
Particle g_particles[300];

// Initialize stars
void initStars() {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < g_starCount; i++) {
        g_stars[i].x = (rand() % 2000 - 1000) / 10.0f;
        g_stars[i].y = (rand() % 1000 - 500) / 10.0f;
        g_stars[i].z = -40.0f - (rand() % 300) / 10.0f;
        g_stars[i].brightness = 0.3f + (rand() % 70) / 100.0f;
    }
}

// Initialize particles
void initParticles() {
    for (int i = 0; i < 300; i++) {
        g_particles[i].x = (rand() % 600 - 300) / 50.0f;
        g_particles[i].y = (rand() % 400) / 50.0f;
        g_particles[i].z = (rand() % 600 - 300) / 50.0f;
        g_particles[i].vx = (rand() % 100 - 50) / 500.0f;
        g_particles[i].vy = (rand() % 100) / 500.0f;
        g_particles[i].vz = (rand() % 100 - 50) / 500.0f;
    }
}

// Draw checkerboard floor
void drawCheckerFloor() {
    glBegin(GL_QUADS);
    for (int i = -12; i < 12; i++) {
        for (int j = -12; j < 12; j++) {
            if ((i + j) % 2 == 0)
                glColor3f(0.92f, 0.88f, 0.78f);
            else
                glColor3f(0.28f, 0.22f, 0.42f);
            
            glVertex3f((float)i, -0.2f, (float)j);
            glVertex3f((float)i+1, -0.2f, (float)j);
            glVertex3f((float)i+1, -0.2f, (float)j+1);
            glVertex3f((float)i, -0.2f, (float)j+1);
        }
    }
    glEnd();
    
    // Grid lines
    glColor3f(0.5f, 0.4f, 0.6f);
    glBegin(GL_LINES);
    for (int i = -12; i <= 12; i++) {
        glVertex3f((float)i, -0.19f, -12.0f);
        glVertex3f((float)i, -0.19f, 12.0f);
        glVertex3f(-12.0f, -0.19f, (float)i);
        glVertex3f(12.0f, -0.19f, (float)i);
    }
    glEnd();
}

// Draw shiny sphere
void drawShinySphere(float x, float y, float z, float radius) {
    const int slices = 80;
    const int stacks = 80;
    
    for (int i = 0; i < stacks; i++) {
        float phi1 = 3.14159f * i / stacks;
        float phi2 = 3.14159f * (i+1) / stacks;
        float y1 = radius * cos(phi1);
        float y2 = radius * cos(phi2);
        float r1 = radius * sin(phi1);
        float r2 = radius * sin(phi2);
        
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float theta = 2 * 3.14159f * j / slices;
            float x1 = r1 * cos(theta);
            float z1 = r1 * sin(theta);
            float x2 = r2 * cos(theta);
            float z2 = r2 * sin(theta);
            
            // Lighting calculation
            vec3 lightDir;
            lightDir.x = 5 * sin(g_lightAngle);
            lightDir.y = 8 + sin(g_lightAngle*2);
            lightDir.z = 5 * cos(g_lightAngle);
            float len = sqrt(lightDir.x*lightDir.x + lightDir.y*lightDir.y + lightDir.z*lightDir.z);
            lightDir.x /= len; lightDir.y /= len; lightDir.z /= len;
            
            // Normals
            vec3 norm1, norm2;
            norm1.x = x1; norm1.y = y1; norm1.z = z1;
            norm2.x = x2; norm2.y = y2; norm2.z = z2;
            len = sqrt(norm1.x*norm1.x + norm1.y*norm1.y + norm1.z*norm1.z);
            norm1.x /= len; norm1.y /= len; norm1.z /= len;
            len = sqrt(norm2.x*norm2.x + norm2.y*norm2.y + norm2.z*norm2.z);
            norm2.x /= len; norm2.y /= len; norm2.z /= len;
            
            float diff1 = norm1.x*lightDir.x + norm1.y*lightDir.y + norm1.z*lightDir.z;
            float diff2 = norm2.x*lightDir.x + norm2.y*lightDir.y + norm2.z*lightDir.z;
            if (diff1 < 0.15f) diff1 = 0.15f;
            if (diff2 < 0.15f) diff2 = 0.15f;
            
            float shine = 0.85f + 0.15f * sin(g_rotation * 3);
            
            glColor3f(0.88f * diff1 * shine, 0.85f * diff1 * shine, 0.82f * diff1 * shine);
            glVertex3f(x + x1, y + y1, z + z1);
            
            glColor3f(0.88f * diff2 * shine, 0.85f * diff2 * shine, 0.82f * diff2 * shine);
            glVertex3f(x + x2, y + y2, z + z2);
        }
        glEnd();
    }
}

// Draw stars
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

// Draw floating particles
void drawFloatingParticles() {
    glDisable(GL_LIGHTING);
    glPointSize(2.5f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 300; i++) {
        g_particles[i].x += g_particles[i].vx;
        g_particles[i].y += g_particles[i].vy;
        g_particles[i].z += g_particles[i].vz;
        
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

// Handle keyboard input
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

// Render scene
void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // Camera
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
    
    // Draw
    drawStars();
    drawFloatingParticles();
    drawShinySphere(0, 1.2f, 0, 1.25f);
    drawCheckerFloor();
    
    g_rotation += 0.02f;
    g_time += 0.016f;
    
    SwapBuffers(g_hdc);
}

// Setup OpenGL
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

// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RetroCGI";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    
    // Create window
    g_hwnd = CreateWindowEx(0, "RetroCGI", 
                            "3DSOFT.EXE - Retro CGI | Shiny Sphere | Checker Floor | Starry Sky",
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
    
    // Initialize
    initGL();
    initStars();
    initParticles();
    
    // Hide cursor
    SetCursorPos(g_width/2, g_height/2);
    ShowCursor(FALSE);
    
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
        Sleep(16);
    }
    
    return 0;
}
