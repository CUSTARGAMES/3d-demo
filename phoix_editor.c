// phoix_editor.c - Phoix 3D Editor (Pure C, 640x480, 256 colors VGA)
// Compile: gcc -o phoix_editor.exe phoix_editor.c -lopengl32 -lglu32 -lgdi32 -lm -mwindows -O3

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// VGA 256 COLOR PALETTE
// ============================================================================
typedef struct { unsigned char r, g, b; } RGBColor;

RGBColor VGA_PALETTE[256] = {
    {0,0,0}, {0,0,170}, {0,170,0}, {0,170,170},
    {170,0,0}, {170,0,170}, {170,85,0}, {170,170,170},
    {85,85,85}, {85,85,255}, {85,255,85}, {85,255,255},
    {255,85,85}, {255,85,255}, {255,255,85}, {255,255,255}
};

// Generate 6x6x6 color cube for remaining 240 colors
void initVGAPalette() {
    int idx = 16;
    for(int r = 0; r < 6 && idx < 256; r++) {
        for(int g = 0; g < 6 && idx < 256; g++) {
            for(int b = 0; b < 6 && idx < 256; b++) {
                VGA_PALETTE[idx++] = {r*51, g*51, b*51};
            }
        }
    }
}

int rgbToVGA(float r, float g, float b) {
    int ir = (int)(r * 255);
    int ig = (int)(g * 255);
    int ib = (int)(b * 255);
    int best = 0;
    int bestDist = 999999;
    
    for(int i = 0; i < 256; i++) {
        int dr = ir - VGA_PALETTE[i].r;
        int dg = ig - VGA_PALETTE[i].g;
        int db = ib - VGA_PALETTE[i].b;
        int dist = dr*dr + dg*dg + db*db;
        if(dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

// ============================================================================
// MATH
// ============================================================================
typedef struct { double x, y, z; } Vec3;

Vec3 vec3(double x, double y, double z) { Vec3 v = {x, y, z}; return v; }
Vec3 add(Vec3 a, Vec3 b) { return vec3(a.x+b.x, a.y+b.y, a.z+b.z); }
Vec3 sub(Vec3 a, Vec3 b) { return vec3(a.x-b.x, a.y-b.y, a.z-b.z); }
Vec3 mul(Vec3 v, double s) { return vec3(v.x*s, v.y*s, v.z*s); }
double dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x); }
Vec3 normalize(Vec3 v) { double len = sqrt(dot(v,v)); return len>0 ? vec3(v.x/len, v.y/len, v.z/len) : vec3(0,0,0); }
double length(Vec3 v) { return sqrt(dot(v,v)); }

// ============================================================================
// SCENE DATA
// ============================================================================
#define MAX_OBJECTS 100
#define MAX_TRIANGLES 10000

typedef struct {
    Vec3 pos;
    Vec3 color;
    float metalness;
    float roughness;
} Sphere;

typedef struct {
    Vec3 v0, v1, v2;
    Vec3 normal;
    Vec3 color;
} Triangle;

typedef struct {
    Vec3 pos;
    Vec3 color;
    float intensity;
} Light;

typedef struct {
    Vec3 position;
    Vec3 target;
    float fov;
} Camera;

// Scene globals
static Sphere g_spheres[MAX_OBJECTS];
static int g_sphereCount = 0;
static Triangle g_triangles[MAX_TRIANGLES];
static int g_triangleCount = 0;
static Light g_lights[MAX_OBJECTS];
static int g_lightCount = 0;
static Camera g_camera = {{0, 1.5, 6}, {0, 1.2, 0}, 60};
static unsigned char* g_frameBuffer = NULL;
static int g_rendering = 0;
static float g_renderProgress = 0;

// Skybox
static unsigned char* g_skyboxData = NULL;
static int g_skyboxWidth = 0;
static int g_skyboxHeight = 0;

// ============================================================================
// GLB MODEL LOADER (Simple)
// ============================================================================
int loadGLB(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if(!f) return 0;
    
    // Read GLB header
    unsigned int magic, version, length;
    fread(&magic, 4, 1, f);
    fread(&version, 4, 1, f);
    fread(&length, 4, 1, f);
    
    if(magic != 0x46546C67) { // "glTF"
        fclose(f);
        return 0;
    }
    
    // Skip to JSON chunk
    unsigned int chunkLength, chunkType;
    fread(&chunkLength, 4, 1, f);
    fread(&chunkType, 4, 1, f);
    
    // Read JSON
    char* jsonData = (char*)malloc(chunkLength + 1);
    fread(jsonData, 1, chunkLength, f);
    jsonData[chunkLength] = 0;
    
    // Simple parsing - just look for "position" data
    // For now, just add a default sphere
    fclose(f);
    free(jsonData);
    
    // Add as sphere for now
    g_spheres[g_sphereCount++] = (Sphere){{0, 1.2, 0}, {0.9, 0.85, 0.8}, 0.9, 0.2};
    return 1;
}

// ============================================================================
// SKYBOX LOADER
// ============================================================================
int loadSkybox(const char* filename) {
    // Simple BMP loader for skybox
    FILE* f = fopen(filename, "rb");
    if(!f) return 0;
    
    BITMAPFILEHEADER bf;
    BITMAPINFOHEADER bi;
    fread(&bf, sizeof(bf), 1, f);
    fread(&bi, sizeof(bi), 1, f);
    
    if(bf.bfType != 0x4D42) {
        fclose(f);
        return 0;
    }
    
    g_skyboxWidth = bi.biWidth;
    g_skyboxHeight = bi.biHeight;
    g_skyboxData = (unsigned char*)malloc(g_skyboxWidth * g_skyboxHeight * 3);
    
    fseek(f, bf.bfOffBits, SEEK_SET);
    fread(g_skyboxData, 3, g_skyboxWidth * g_skyboxHeight, f);
    fclose(f);
    return 1;
}

// ============================================================================
// RAY TRACING ENGINE
// ============================================================================
typedef struct { Vec3 point, normal; double t; int hit; int isSphere; } Hit;

Hit sphereIntersect(Sphere s, Vec3 origin, Vec3 dir) {
    Hit hit = {0};
    Vec3 oc = sub(origin, s.pos);
    double a = dot(dir, dir);
    double b = 2.0 * dot(oc, dir);
    double c = dot(oc, oc) - 1.2*1.2;
    double disc = b*b - 4*a*c;
    
    if(disc > 0) {
        double t = (-b - sqrt(disc)) / (2*a);
        if(t > 0.001) {
            hit.hit = 1;
            hit.t = t;
            hit.point = add(origin, mul(dir, t));
            hit.normal = normalize(sub(hit.point, s.pos));
            hit.isSphere = 1;
        }
    }
    return hit;
}

Hit planeIntersect(Vec3 point, Vec3 normal, Vec3 origin, Vec3 dir) {
    Hit hit = {0};
    double denom = dot(normal, dir);
    if(fabs(denom) > 1e-6) {
        double t = dot(sub(point, origin), normal) / denom;
        if(t > 0.001) {
            hit.hit = 1;
            hit.t = t;
            hit.point = add(origin, mul(dir, t));
            hit.normal = normal;
            hit.isSphere = 0;
        }
    }
    return hit;
}

Vec3 traceRay(Vec3 origin, Vec3 dir, int depth) {
    if(depth > 5) return vec3(0, 0, 0);
    
    // Find closest hit
    Hit closest = {0};
    closest.t = 1e6;
    
    // Check spheres
    for(int i = 0; i < g_sphereCount; i++) {
        Hit h = sphereIntersect(g_spheres[i], origin, dir);
        if(h.hit && h.t < closest.t) {
            closest = h;
        }
    }
    
    // Check floor
    Hit floorHit = planeIntersect(vec3(0, -0.2, 0), vec3(0, 1, 0), origin, dir);
    if(floorHit.hit && floorHit.t < closest.t) {
        closest = floorHit;
    }
    
    if(!closest.hit) {
        // Sky
        if(g_skyboxData) {
            // Simple skybox mapping
            return vec3(0.5, 0.6, 0.8);
        }
        double t = 0.5 * (dir.y + 1.0);
        return vec3(0.02 + t*0.01, 0.03 + t*0.02, 0.12 + t*0.08);
    }
    
    // Lighting
    Vec3 lightDir = normalize(sub(g_lights[0].pos, closest.point));
    double diff = fmax(0.1, dot(closest.normal, lightDir));
    
    // Shadow
    Hit shadowHit = sphereIntersect(g_spheres[0], closest.point, lightDir);
    double shadow = (shadowHit.hit && shadowHit.t > 0.01) ? 0.4 : 1.0;
    diff *= shadow;
    
    Vec3 color;
    if(closest.isSphere) {
        // Shiny sphere
        Vec3 sphereColor = vec3(0.92, 0.88, 0.82);
        Vec3 viewDir = normalize(mul(dir, -1));
        Vec3 reflectDir = normalize(sub(mul(lightDir, -1), mul(closest.normal, 2*dot(mul(lightDir, -1), closest.normal))));
        double spec = pow(fmax(0, dot(viewDir, reflectDir)), 64);
        
        color = add(mul(sphereColor, diff * 0.85 + 0.15), mul(vec3(1,0.95,0.9), spec * 0.9 * shadow));
        
        // Reflection
        Vec3 reflectDir2 = normalize(sub(dir, mul(closest.normal, 2*dot(dir, closest.normal))));
        Vec3 reflectColor = traceRay(closest.point, reflectDir2, depth + 1);
        color = add(color, mul(reflectColor, 0.5));
        
        // Rim light
        double rim = pow(1 - fmax(0, dot(viewDir, closest.normal)), 3);
        color = add(color, mul(vec3(0.7, 0.5, 0.4), rim * 0.4));
    } else {
        // Floor - checker
        int x = (int)floor(closest.point.x * 2.0);
        int z = (int)floor(closest.point.z * 2.0);
        if((x + z) % 2 == 0)
            color = vec3(0.95, 0.90, 0.85);
        else
            color = vec3(0.35, 0.28, 0.52);
        color = mul(color, diff * 0.9 + 0.2);
    }
    
    return color;
}

// ============================================================================
// RENDER FUNCTION
// ============================================================================
void renderScene() {
    if(g_rendering) return;
    g_rendering = 1;
    g_renderProgress = 0;
    
    if(!g_frameBuffer) {
        g_frameBuffer = (unsigned char*)malloc(640 * 480 * 3);
    }
    
    Vec3 camPos = g_camera.position;
    Vec3 camTarget = g_camera.target;
    Vec3 camDir = normalize(sub(camTarget, camPos));
    Vec3 camRight = normalize(cross(camDir, vec3(0, 1, 0)));
    Vec3 camUp = normalize(cross(camRight, camDir));
    
    double fovRad = g_camera.fov * 3.14159 / 180.0;
    double aspect = 640.0 / 480.0;
    
    for(int y = 0; y < 480; y++) {
        if(y % 50 == 0) {
            g_renderProgress = (float)y / 480;
            InvalidateRect(GetForegroundWindow(), NULL, FALSE);
        }
        
        for(int x = 0; x < 640; x++) {
            double u = (2.0 * x / 640 - 1.0) * tan(fovRad/2) * aspect;
            double v = (1.0 - 2.0 * y / 480) * tan(fovRad/2);
            
            Vec3 rayDir = normalize(add(add(camDir, mul(camRight, u)), mul(camUp, v)));
            Vec3 color = traceRay(camPos, rayDir, 0);
            
            // Clamp
            if(color.x > 1) color.x = 1; if(color.x < 0) color.x = 0;
            if(color.y > 1) color.y = 1; if(color.y < 0) color.y = 0;
            if(color.z > 1) color.z = 1; if(color.z < 0) color.z = 0;
            
            g_frameBuffer[(y * 640 + x) * 3 + 0] = (unsigned char)(color.x * 255);
            g_frameBuffer[(y * 640 + x) * 3 + 1] = (unsigned char)(color.y * 255);
            g_frameBuffer[(y * 640 + x) * 3 + 2] = (unsigned char)(color.z * 255);
        }
    }
    
    g_rendering = 0;
    InvalidateRect(GetForegroundWindow(), NULL, FALSE);
}

// ============================================================================
// WINDOW PROCEDURE
// ============================================================================
HWND g_hwnd;
HDC g_hdc;
int g_width = 640;
int g_height = 480;
float g_camAngleX = 0, g_camAngleY = 0.3f;
int g_mouseX = 320, g_mouseY = 240;
int g_firstMouse = 1;
int g_keys[256] = {0};
int g_dragging = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            g_keys[wParam] = 1;
            if(wParam == VK_ESCAPE) PostQuitMessage(0);
            if(wParam == 'R') {
                // Reset camera
                g_camera.position = vec3(0, 1.5, 6);
                g_camera.target = vec3(0, 1.2, 0);
                renderScene();
            }
            if(wParam == 'F') {
                // Load GLB model
                OPENFILENAME ofn = {0};
                char file[MAX_PATH] = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = "GLB Files\0*.glb\0";
                ofn.lpstrFile = file;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST;
                
                if(GetOpenFileName(&ofn)) {
                    loadGLB(file);
                    renderScene();
                }
            }
            if(wParam == 'S') {
                // Load skybox
                OPENFILENAME ofn = {0};
                char file[MAX_PATH] = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = "BMP Files\0*.bmp\0";
                ofn.lpstrFile = file;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST;
                
                if(GetOpenFileName(&ofn)) {
                    loadSkybox(file);
                    renderScene();
                }
            }
            return 0;
        case WM_KEYUP:
            g_keys[wParam] = 0;
            return 0;
        case WM_MOUSEMOVE:
            if(g_dragging) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                int dx = x - g_mouseX;
                int dy = y - g_mouseY;
                
                // Orbit camera
                g_camAngleX += dx * 0.005f;
                g_camAngleY -= dy * 0.003f;
                if(g_camAngleY > 1.4f) g_camAngleY = 1.4f;
                if(g_camAngleY < -0.8f) g_camAngleY = -0.8f;
                
                // Update camera position
                float dist = length(sub(g_camera.position, g_camera.target));
                g_camera.position.x = g_camera.target.x + dist * sin(g_camAngleX) * cos(g_camAngleY);
                g_camera.position.y = g_camera.target.y + dist * sin(g_camAngleY);
                g_camera.position.z = g_camera.target.z + dist * cos(g_camAngleX) * cos(g_camAngleY);
                
                g_mouseX = x; g_mouseY = y;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN:
            g_dragging = 1;
            g_mouseX = LOWORD(lParam);
            g_mouseY = HIWORD(lParam);
            SetCapture(hwnd);
            return 0;
        case WM_LBUTTONUP:
            g_dragging = 0;
            ReleaseCapture();
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            if(g_frameBuffer && !g_rendering) {
                // Convert to VGA palette and display
                static unsigned char vgaBuffer[640*480];
                for(int i = 0; i < 640*480; i++) {
                    float r = g_frameBuffer[i*3+0] / 255.0f;
                    float g = g_frameBuffer[i*3+1] / 255.0f;
                    float b = g_frameBuffer[i*3+2] / 255.0f;
                    vgaBuffer[i] = (unsigned char)rgbToVGA(r, g, b);
                }
                
                // Create bitmap from VGA palette
                HBITMAP hBitmap = CreateBitmap(640, 480, 1, 8, vgaBuffer);
                HDC memDC = CreateCompatibleDC(hdc);
                SelectObject(memDC, hBitmap);
                
                // Set palette
                LOGPALETTE* pPal = (LOGPALETTE*)malloc(sizeof(LOGPALETTE) + 256 * sizeof(PALETTEENTRY));
                pPal->palVersion = 0x300;
                pPal->palNumEntries = 256;
                for(int i = 0; i < 256; i++) {
                    pPal->palPalEntry[i].peRed = VGA_PALETTE[i].r;
                    pPal->palPalEntry[i].peGreen = VGA_PALETTE[i].g;
                    pPal->palPalEntry[i].peBlue = VGA_PALETTE[i].b;
                    pPal->palPalEntry[i].peFlags = 0;
                }
                HPALETTE hPal = CreatePalette(pPal);
                free(pPal);
                
                SelectPalette(hdc, hPal, FALSE);
                RealizePalette(hdc);
                
                BitBlt(hdc, 0, 0, 640, 480, memDC, 0, 0, SRCCOPY);
                
                DeleteDC(memDC);
                DeleteObject(hBitmap);
                DeleteObject(hPal);
            }
            
            // Progress bar
            if(g_rendering) {
                HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
                RECT rect = {200, 240, 440, 260};
                FillRect(hdc, &rect, hBrush);
                DeleteObject(hBrush);
                
                hBrush = CreateSolidBrush(RGB(200, 150, 50));
                rect.left = 202;
                rect.right = 202 + (int)(236 * g_renderProgress);
                rect.top = 242;
                rect.bottom = 258;
                FillRect(hdc, &rect, hBrush);
                DeleteObject(hBrush);
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// MAIN
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    initVGAPalette();
    
    // Default scene
    g_spheres[0] = (Sphere){{0, 1.2, 0}, {0.92, 0.88, 0.82}, 0.9, 0.2};
    g_sphereCount = 1;
    g_lights[0] = (Light){{5, 8, 3}, {1, 0.95, 0.9}, 1};
    g_lightCount = 1;
    
    // Register window
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "Phoix3DEditor";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClass(&wc);
    
    // Create 640x480 window
    g_hwnd = CreateWindowEx(0, "Phoix3DEditor", 
        "Phoix 3D Editor | Drag to orbit | F=Load GLB | S=Skybox | R=Reset",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
        100, 100, 640, 480,
        NULL, NULL, hInstance, NULL);
    
    if(!g_hwnd) return 1;
    
    // Initial render
    renderScene();
    
    // Message loop
    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
