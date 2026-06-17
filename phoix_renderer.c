// phoix_renderer.c - Phoix Standalone Render Engine
// Compile: gcc -o phoix_renderer.exe phoix_renderer.c -lm -O3

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

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

// ============================================================================
// SCENE
// ============================================================================
typedef struct { Vec3 pos; Vec3 color; double metalness; double roughness; } Sphere;
typedef struct { Vec3 pos; Vec3 color; double intensity; } Light;

static Sphere g_spheres[100];
static int g_sphereCount = 0;
static Light g_lights[100];
static int g_lightCount = 0;

// ============================================================================
// RAY TRACING
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
    
    Hit closest = {0};
    closest.t = 1e6;
    
    for(int i = 0; i < g_sphereCount; i++) {
        Hit h = sphereIntersect(g_spheres[i], origin, dir);
        if(h.hit && h.t < closest.t) {
            closest = h;
        }
    }
    
    Hit floorHit = planeIntersect(vec3(0, -0.2, 0), vec3(0, 1, 0), origin, dir);
    if(floorHit.hit && floorHit.t < closest.t) {
        closest = floorHit;
    }
    
    if(!closest.hit) {
        double t = 0.5 * (dir.y + 1.0);
        return vec3(0.02 + t*0.01, 0.03 + t*0.02, 0.12 + t*0.08);
    }
    
    Vec3 lightDir = normalize(sub(g_lights[0].pos, closest.point));
    double diff = fmax(0.1, dot(closest.normal, lightDir));
    
    Hit shadowHit = sphereIntersect(g_spheres[0], closest.point, lightDir);
    double shadow = (shadowHit.hit && shadowHit.t > 0.01) ? 0.4 : 1.0;
    diff *= shadow;
    
    Vec3 color;
    if(closest.isSphere) {
        Vec3 sphereColor = vec3(0.92, 0.88, 0.82);
        Vec3 viewDir = normalize(mul(dir, -1));
        Vec3 reflectDir = normalize(sub(mul(lightDir, -1), mul(closest.normal, 2*dot(mul(lightDir, -1), closest.normal))));
        double spec = pow(fmax(0, dot(viewDir, reflectDir)), 64);
        
        color = add(mul(sphereColor, diff * 0.85 + 0.15), mul(vec3(1,0.95,0.9), spec * 0.9 * shadow));
        
        Vec3 reflectDir2 = normalize(sub(dir, mul(closest.normal, 2*dot(dir, closest.normal))));
        Vec3 reflectColor = traceRay(closest.point, reflectDir2, depth + 1);
        color = add(color, mul(reflectColor, 0.5));
        
        double rim = pow(1 - fmax(0, dot(viewDir, closest.normal)), 3);
        color = add(color, mul(vec3(0.7, 0.5, 0.4), rim * 0.4));
    } else {
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
// MAIN RENDER FUNCTION
// ============================================================================
void renderToPPM(const char* filename, int width, int height) {
    Vec3 camPos = vec3(0, 1.5, 6);
    Vec3 camTarget = vec3(0, 1.2, 0);
    Vec3 camDir = normalize(sub(camTarget, camPos));
    Vec3 camRight = normalize(cross(camDir, vec3(0, 1, 0)));
    Vec3 camUp = normalize(cross(camRight, camDir));
    
    double fovRad = 60.0 * 3.14159 / 180.0;
    double aspect = (double)width / height;
    
    FILE* f = fopen(filename, "w");
    if(!f) return;
    
    fprintf(f, "P3\n%d %d\n255\n", width, height);
    
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            double u = (2.0 * x / width - 1.0) * tan(fovRad/2) * aspect;
            double v = (1.0 - 2.0 * y / height) * tan(fovRad/2);
            
            Vec3 rayDir = normalize(add(add(camDir, mul(camRight, u)), mul(camUp, v)));
            Vec3 color = traceRay(camPos, rayDir, 0);
            
            if(color.x > 1) color.x = 1; if(color.x < 0) color.x = 0;
            if(color.y > 1) color.y = 1; if(color.y < 0) color.y = 0;
            if(color.z > 1) color.z = 1; if(color.z < 0) color.z = 0;
            
            fprintf(f, "%d %d %d\n", 
                (int)(color.x * 255),
                (int)(color.y * 255),
                (int)(color.z * 255));
        }
    }
    
    fclose(f);
}

int main(int argc, char** argv) {
    // Default scene
    g_spheres[0] = (Sphere){{0, 1.2, 0}, {0.92, 0.88, 0.82}, 0.9, 0.2};
    g_sphereCount = 1;
    g_lights[0] = (Light){{5, 8, 3}, {1, 0.95, 0.9}, 1};
    g_lightCount = 1;
    
    int width = 640;
    int height = 480;
    char* output = "render.ppm";
    
    // Parse command line
    if(argc >= 2) {
        output = argv[1];
    }
    if(argc >= 4) {
        width = atoi(argv[2]);
        height = atoi(argv[3]);
    }
    
    printf("Phoix Render Engine\n");
    printf("Rendering %dx%d to %s\n", width, height, output);
    
    clock_t start = clock();
    renderToPPM(output, width, height);
    clock_t end = clock();
    
    printf("Done! Time: %.2f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);
    
    return 0;
}
