// cube3d_windows.c
#include <windows.h>
#include <math.h>
#include <stdio.h>

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600

// Cube vertices (8 corners)
typedef struct {
    float x, y, z;
} Vec3;

Vec3 cubeVertices[8] = {
    {-1, -1, -1}, { 1, -1, -1},
    { 1,  1, -1}, {-1,  1, -1},
    {-1, -1,  1}, { 1, -1,  1},
    { 1,  1,  1}, {-1,  1,  1}
};

// Edge connections (12 edges)
int edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
};

// Projected 2D points
POINT proj[8];

// Rotation angles
float angleX = 0, angleY = 0, angleZ = 0;
int running = 1;

// PORTABLE 3D MATH - Copy this to your OS!
void rotateAndProject() {
    float cosX = cos(angleX), sinX = sin(angleX);
    float cosY = cos(angleY), sinY = sin(angleY);
    float cosZ = cos(angleZ), sinZ = sin(angleZ);
    
    int originX = SCREEN_WIDTH / 2;
    int originY = SCREEN_HEIGHT / 2;
    float focal = 500;
    
    for (int i = 0; i < 8; i++) {
        float x = cubeVertices[i].x;
        float y = cubeVertices[i].y;
        float z = cubeVertices[i].z;
        
        // Rotate around X
        float y1 = y * cosX - z * sinX;
        float z1 = y * sinX + z * cosX;
        
        // Rotate around Y
        float x2 = x * cosY + z1 * sinY;
        float z2 = -x * sinY + z1 * cosY;
        
        // Rotate around Z
        float x3 = x2 * cosZ - y1 * sinZ;
        float y3 = x2 * sinZ + y1 * cosZ;
        
        // Perspective projection
        float z3 = z2 + 3.0;
        if (z3 > 0.1) {
            proj[i].x = originX + (int)(focal * x3 / z3);
            proj[i].y = originY - (int)(focal * y3 / z3);
        }
    }
}

// Drawing functions (replace these for your OS)
void drawLine(HDC hdc, int x1, int y1, int x2, int y2) {
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
}

void drawCube(HDC hdc) {
    HPEN pen = CreatePen(PS_SOLID, 3, RGB(0, 255, 0));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    for (int i = 0; i < 12; i++) {
        drawLine(hdc, proj[edges[i][0]].x, proj[edges[i][0]].y,
                      proj[edges[i][1]].x, proj[edges[i][1]].y);
    }
    
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

// Windows Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_DESTROY:
            running = 0;
            PostQuitMessage(0);
            return 0;
            
        case WM_KEYDOWN:
            switch(wParam) {
                case VK_UP:    angleX += 0.05; break;
                case VK_DOWN:  angleX -= 0.05; break;
                case VK_LEFT:  angleY -= 0.05; break;
                case VK_RIGHT: angleY += 0.05; break;
                case 'W':      angleX += 0.05; break;
                case 'S':      angleX -= 0.05; break;
                case 'A':      angleY -= 0.05; break;
                case 'D':      angleY += 0.05; break;
                case 'Q':
                case VK_ESCAPE: 
                    running = 0;
                    PostQuitMessage(0);
                    break;
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "Cube3D";
    RegisterClass(&wc);
    
    // Create window
    HWND hwnd = CreateWindow("Cube3D", "3D Cube - Portable Math - Use Arrow Keys/WASD",
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             SCREEN_WIDTH, SCREEN_HEIGHT,
                             NULL, NULL, hInstance, NULL);
    
    if (!hwnd) return 0;
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    // Message loop with animation
    MSG msg;
    while (running && GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        // Update 3D projection
        rotateAndProject();
        
        // Force redraw
        InvalidateRect(hwnd, NULL, TRUE);
        
        // Control frame rate
        Sleep(16);
    }
    
    return msg.wParam;
}
