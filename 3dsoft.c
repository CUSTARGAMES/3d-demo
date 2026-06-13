// 3dsoft.c - Hybrid Real-time + Raytraced Renderer
// Compile: gcc -o 3dsoft.exe 3dsoft.c -lopengl32 -lglu32 -lm -mwindows

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
// MATH
// ============================================================================
typedef struct { double x, y, z; } Vec3;
typedef struct { Vec3 origin, direction; } Ray;
typedef struct { Vec3 point, normal; double t; int hit; } Hit;

Vec3 vec3(double x, double y, double z) { Vec3 v = {x, y, z}; return v; }
Vec3 add(Vec3 a, Vec3 b) { return vec3(a.x+b.x, a.y+b.y, a.z+b.z); }
Vec3 sub(Vec3 a, Vec3 b) { return vec3(a.x-b.x, a.y-b.y, a.z-b.z); }
Vec3 mul(Vec3 v, double s) { return vec3(v.x*s, v.y*s, v.z*s); }
double dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3 normalize(Vec3 v) { double len = sqrt(dot(v,v)); return mul(v, 1.0/len); }

// ============================================================================
// GLOBALS
// ============================================================================
HWND g_hwnd;
HDC g_hdc;
HGLRC g_hrc;
int g_width = 1024;
int g_height = 768;
float g_camX = 0, g_camY = 1.5f, g_camZ = 6.0f;
float g_camAngleX = 0, g_camAngleY = 0.3f;
int g_keys[256] = {0};
int g_mouseX = 512, g_mouseY = 384;
int g_firstMouse = 1;
int g_rendering = 0;
double g_renderProgress = 0;

// Stars
typedef struct { float x, y, z; float bright; } Star;
Star g_stars[2000];
int g_starCount = 2000;

// ============================================================================
// RAY TRACING FUNCTIONS (for the "R" render)
// ============================================================================
Hit sphereIntersect(Vec3 center, double radius, Ray ray) {
    Hit hit = {0};
    Vec3 oc = sub(ray.origin, center);
    double a = dot(ray.direction, ray.direction);
    double b = 2.0 * dot(oc, ray.direction);
    double c = dot(oc, oc) - radius*radius;
    double disc = b*b - 4*a*c;
    
    if (disc > 0) {
        double t = (-b - sqrt(disc)) / (2*a);
        if (t > 0.001) {
            hit.hit = 1;
            hit.t = t;
            hit.point = add(ray.origin, mul(ray.direction, t));
            hit.normal = normalize(sub(hit.point, center));
        }
    }
    return hit;
}

Hit planeIntersect(Vec3 point, Vec3 normal, Ray ray) {
    Hit hit = {0};
    double denom = dot(normal, ray.direction);
    if (fabs(denom) > 1e-6) {
        double t = dot(sub(point, ray.origin), normal) / denom;
        if (t > 0.001) {
            hit.hit = 1;
            hit.t = t;
            hit.point = add(ray.origin, mul(ray.direction, t));
            hit.normal = normal;
        }
    }
    return hit;
}

Vec3 checkerColor(Vec3 point) {
    int x = (int)floor(point.x * 2.0);
    int z = (int)floor(point.z * 2.0);
    if ((x + z) % 2 == 0)
        return vec3(0.95, 0.90, 0.85);
    else
        return vec3(0.35, 0.28, 0.52);
}

Vec3 traceRay(Ray ray, int depth) {
    if (depth > 5) return vec3(0,0,0);
    
    // Sphere intersection
    Hit sphereHit = sphereIntersect(vec3(0, 1.2, 0), 1.25, ray);
    // Floor intersection
    Hit floorHit = planeIntersect(vec3(0, -0.2, 0), vec3(0, 1, 0), ray);
    
    Hit hit = {0};
    int hitSphere = 0;
    
    if (sphereHit.hit && (!floorHit.hit || sphereHit.t < floorHit.t)) {
        hit = sphereHit;
        hitSphere = 1;
    } else if (floorHit.hit) {
        hit = floorHit;
    }
    
    if (!hit.hit) {
        // Starry sky
        double t = 0.5 * (ray.direction.y + 1.0);
        return vec3(0.02 + t*0.01, 0.03 + t*0.02, 0.12 + t*0.08);
    }
    
    // Lighting
    Vec3 lightPos = vec3(5 * sin(g_renderProgress*2), 8, 5 * cos(g_renderProgress*2));
    Vec3 lightDir = normalize(sub(lightPos, hit.point));
    
    // Shadow ray
    Ray shadowRay = {hit.point, lightDir};
    Hit shadowHit = sphereIntersect(vec3(0, 1.2, 0), 1.25, shadowRay);
    double shadowIntensity = (shadowHit.hit && shadowHit.t > 0.01) ? 0.3 : 1.0;
    
    double diff = fmax(0.1, dot(hit.normal, lightDir)) * shadowIntensity;
    
    if (!hitSphere) {
        // Floor
        Vec3 color = checkerColor(hit.point);
        return mul(color, diff * 0.9 + 0.2);
    }
    
    // SPHERE - SHINY METALLIC with reflections!
    Vec3 sphereColor = vec3(0.92, 0.88, 0.82);
    
    // Specular highlight
    Vec3 viewDir = normalize(mul(ray.direction, -1));
    Vec3 reflectDir = normalize(sub(mul(lightDir, -1), mul(hit.normal, 2*dot(mul(lightDir, -1), hit.normal))));
    double spec = pow(fmax(0, dot(viewDir, reflectDir)), 128);
    Vec3 specular = mul(vec3(1.0, 0.95, 0.90), spec * 0.9 * shadowIntensity);
    
    // Base color
    Vec3 color = add(mul(sphereColor, diff * 0.85 + 0.15), specular);
    
    // REFLECTION RAY (this makes it BLENDER-LEVEL shiny!)
    Ray reflectRay = {hit.point, normalize(sub(ray.direction, mul(hit.normal, 2*dot(ray.direction, hit.normal))))};
    Vec3 reflectColor = traceRay(reflectRay, depth + 1);
    color = add(color, mul(reflectColor, 0.55));
    
    // Rim lighting
    double rim = pow(1 - fmax(0, dot(viewDir, hit.normal)), 3);
    color = add(color, mul(vec3(0.7, 0.5, 0.4), rim * 0.4));
    
    return color;
}

void renderHighQuality() {
    g_rendering = 1;
    g_renderProgress = 0;
    
    int renderWidth = 1920;
    int renderHeight = 1080;
    unsigned char* pixels = (unsigned char*)malloc(renderWidth * renderHeight * 3);
    
    // Camera position from current view
    Vec3 camPos = vec3(g_camX, g_camY, g_camZ);
    Vec3 camDir = vec3(sin(g_camAngleX), sin(g_camAngleY), cos(g_camAngleX));
    Vec3 camRight = normalize(vec3(-camDir.z, 0, camDir.x));
    Vec3 camUp = normalize(cross(camRight, camDir));
    
    double fov = 60.0 * 3.14159 / 180.0;
    double aspect = (double)renderWidth / renderHeight;
    
    for (int y = 0; y < renderHeight; y++) {
        if (y % 50 == 0) {
            g_renderProgress = (double)y / renderHeight;
            InvalidateRect(g_hwnd, NULL, FALSE);
        }
        
        for (int x = 0; x < renderWidth; x++) {
            double u = (2.0 * x / renderWidth - 1.0) * tan(fov/2) * aspect;
            double v = (1.0 - 2.0 * y / renderHeight) * tan(fov/2);
            
            Vec3 rayDir = normalize(add(add(camDir, mul(camRight, u)), mul(camUp, v)));
            Ray ray = {camPos, rayDir};
            
            Vec3 color = traceRay(ray, 0);
            
            // Gamma correction
            color.x = pow(color.x, 1.0/2.2);
            color.y = pow(color.y, 1.0/2.2);
            color.z = pow(color.z, 1.0/2.2);
            
            pixels[(y * renderWidth + x) * 3 + 0] = (unsigned char)(fmin(1.0, color.x) * 255);
            pixels[(y * renderWidth + x) * 3 + 1] = (unsigned char)(fmin(1.0, color.y) * 255);
            pixels[(y * renderWidth + x) * 3 + 2] = (unsigned char)(fmin(1.0, color.z) * 255);
        }
    }
    
    // Save as BMP
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = renderWidth;
    bmi.bmiHeader.biHeight = -renderHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    HDC hdcScreen = GetDC(NULL);
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, NULL, NULL, 0);
    SetDIBits(hdcScreen, hBitmap, 0, renderHeight, pixels, &bmi, DIB_RGB_COLORS);
    
    OPENFILENAME ofn = {0};
    char fileName[MAX_PATH] = "render_output.bmp";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = "Bitmap Files\0*.bmp\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileName(&ofn)) {
        HBITMAP hBitmapCopy = (HBITMAP)CopyImage(hBitmap, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        SelectObject(hdcMem, hBitmapCopy);
        
        BITMAPFILEHEADER bf = {0};
        BITMAPINFOHEADER bi = {0};
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = renderWidth;
        bi.biHeight = renderHeight;
        bi.biPlanes = 1;
        bi.biBitCount = 24;
        bi.biCompression = BI_RGB;
        
        DWORD dwSize = ((renderWidth * 24 + 31) / 32) * 4 * renderHeight;
        bf.bfType = 0x4D42;
        bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bf.bfSize = bf.bfOffBits + dwSize;
        
        HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(hFile, &bf, sizeof(bf), &written, NULL);
            WriteFile(hFile, &bi, sizeof(bi), &written, NULL);
            
            BITMAP bitmap;
            GetObject(hBitmapCopy, sizeof(BITMAP), &bitmap);
            for (int y = renderHeight-1; y >= 0; y--) {
                WriteFile(hFile, (BYTE*)bitmap.bmBits + y * bitmap.bmWidthBytes, bitmap.bmWidthBytes, &written, NULL);
            }
            CloseHandle(hFile);
            MessageBox(g_hwnd, "Render complete! Image saved.", "3DSOFT.RENDER", MB_OK);
        }
        DeleteDC(hdcMem);
        DeleteObject(hBitmapCopy);
    }
    
    DeleteObject(hBitmap);
    ReleaseDC(NULL, hdcScreen);
    free(pixels);
    g_rendering = 0;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

// ============================================================================
// REAL-TIME OPENGL DRAWING (for movement preview)
// ============================================================================
void drawCheckerFloorGL() {
    glBegin(GL_QUADS);
    for (int i = -12; i < 12; i++) {
        for (int j = -12; j < 12; j++) {
            if ((i + j) % 2 == 0)
                glColor3f(0.92f, 0.88f, 0.78f);
            else
                glColor3f(0.35f, 0.28f, 0.52f);
            glVertex3f((float)i, -0.2f, (float)j);
            glVertex3f((float)i+1, -0.2f, (float)j);
            glVertex3f((float)i+1, -0.2f, (float)j+1);
            glVertex3f((float)i, -0.2f, (float)j+1);
        }
    }
    glEnd();
}

void drawSphereGL(float x, float y, float z, float radius) {
    const int slices = 40;
    const int stacks = 40;
    float lightAngle = (float)g_renderProgress * 6.28318f;
    
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
            
            // Fake shiny lighting
            float lightX = 3 * sin(lightAngle);
            float lightZ = 3 * cos(lightAngle);
            float nx = x1/radius, ny = y1/radius, nz = z1/radius;
            float diff = nx*lightX + ny*5 + nz*lightZ;
            diff = (diff + 1) / 3;
            if (diff < 0.2) diff = 0.2;
            
            glColor3f(0.88f * diff, 0.85f * diff, 0.82f * diff);
            glVertex3f(x + x1, y + y1, z + z1);
            
            nx = x2/radius; ny = y2/radius; nz = z2/radius;
            diff = nx*lightX + ny*5 + nz*lightZ;
            diff = (diff + 1) / 3;
            if (diff < 0.2) diff = 0.2;
            glColor3f(0.88f * diff, 0.85f * diff, 0.82f * diff);
            glVertex3f(x + x2, y + y2, z + z2);
        }
        glEnd();
    }
}

void drawStarsGL() {
    glDisable(GL_DEPTH_TEST);
    glPointSize(1.2f);
    glBegin(GL_POINTS);
    for (int i = 0; i < g_starCount; i++) {
        glColor3f(g_stars[i].bright, g_stars[i].bright*0.85f, g_stars[i].bright);
        glVertex3f(g_stars[i].x, g_stars[i].y, g_stars[i].z);
    }
    glEnd();
    glEnable(GL_DEPTH_TEST);
}

void renderPreview() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    gluLookAt(g_camX, g_camY, g_camZ,
              g_camX + sin(g_camAngleX), g_camY + sin(g_camAngleY), g_camZ + cos(g_camAngleX),
              0, 1, 0);
    
    drawStarsGL();
    drawSphereGL(0, 1.2f, 0, 1.25f);
    drawCheckerFloorGL();
    
    if (g_rendering) {
        // Show progress bar
        glDisable(GL_LIGHTING);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0, g_width, 0, g_height);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        
        glBegin(GL_QUADS);
        glColor3f(0, 0, 0);
        glVertex2f(g_width/2 - 300, g_height - 80);
        glVertex2f(g_width/2 + 300, g_height - 80);
        glVertex2f(g_width/2 + 300, g_height - 60);
        glVertex2f(g_width/2 - 300, g_height - 60);
        
        glColor3f(0.8f, 0.6f, 0.3f);
        glVertex2f(g_width/2 - 298, g_height - 78);
        glVertex2f(g_width/2 - 298 + 596 * g_renderProgress, g_height - 78);
        glVertex2f(g_width/2 - 298 + 596 * g_renderProgress, g_height - 62);
        glVertex2f(g_width/2 - 298, g_height - 62);
        glEnd();
        
        glColor3f(1,1,1);
        glRasterPos2f(g_width/2 - 150, g_height - 50);
        char msg[64];
        sprintf(msg, "RENDERING: %d%%", (int)(g_renderProgress * 100));
        for(char* c = msg; *c; c++) {
            wglUseFontBitmaps(wglGetCurrentDC(), *c, 1, 1000);
            glCallLists(1, GL_UNSIGNED_BYTE, c);
        }
        
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glEnable(GL_LIGHTING);
    }
    
    SwapBuffers(g_hdc);
}

// ============================================================================
// INPUT HANDLING
// ============================================================================
void handleInput() {
    if (g_rendering) return;
    
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
            if (wParam == 'R' && !g_rendering) {
                CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)renderHighQuality, NULL, 0, NULL);
            }
            return 0;
        case WM_KEYUP:
            g_keys[wParam] = 0;
            return 0;
        case WM_MOUSEMOVE:
            if (!g_rendering) {
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
// MAIN
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RayTracer3D";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    
    g_hwnd = CreateWindowEx(0, "RayTracer3D", 
                            "3DSOFT.EXE | Move: WASD + Mouse | PRESS R TO RENDER PHOTOREALISTIC IMAGE",
                            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, g_width, g_height,
                            NULL, NULL, hInstance, NULL);
    
    if (!g_hwnd) return 1;
    
    g_hdc = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32, 0,0,0,0,0,0,0,0,0,0,0,0,0,16,0,0,PFD_MAIN_PLANE,0,0,0,0
    };
    SetPixelFormat(g_hdc, ChoosePixelFormat(g_hdc, &pfd), &pfd);
    g_hrc = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, g_hrc);
    
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.02f, 0.08f, 1.0f);
    
    // Initialize stars
    srand(time(NULL));
    for (int i = 0; i < g_starCount; i++) {
        g_stars[i].x = (rand() % 2000 - 1000) / 10.0f;
        g_stars[i].y = (rand() % 1000 - 500) / 10.0f;
        g_stars[i].z = -50.0f - (rand() % 300) / 10.0f;
        g_stars[i].bright = 0.3f + (rand() % 70) / 100.0f;
    }
    
    SetCursorPos(g_width/2, g_height/2);
    ShowCursor(FALSE);
    
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
        renderPreview();
        Sleep(16);
    }
    
    return 0;
}
