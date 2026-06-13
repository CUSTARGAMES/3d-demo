// render.c - Retro CGI: Shiny Sphere, Checker Floor & Starry Sky
// Compile: gcc -o render render.c -lm -O3
// Run: ./render > output.ppm
// View: open output.ppm (on Mac) or use any image viewer

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <float.h>

#define WIDTH 800
#define HEIGHT 600
#define MAX_BOUNCES 8
#define SAMPLES 4  // Anti-aliasing

typedef struct { double x, y, z; } Vec3;
typedef struct { Vec3 origin, direction; } Ray;
typedef struct { Vec3 hit_point, normal; double t; int hit; } Hit;

// Vector operations
Vec3 vec3(double x, double y, double z) { Vec3 v = {x, y, z}; return v; }
Vec3 add(Vec3 a, Vec3 b) { return vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
Vec3 sub(Vec3 a, Vec3 b) { return vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
Vec3 mul(Vec3 v, double s) { return vec3(v.x * s, v.y * s, v.z * s); }
Vec3 mul_v(Vec3 a, Vec3 b) { return vec3(a.x * b.x, a.y * b.y, a.z * b.z); }
double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x); }
Vec3 normalize(Vec3 v) { double len = sqrt(dot(v, v)); return mul(v, 1.0/len); }
double length(Vec3 v) { return sqrt(dot(v, v)); }

// Random utilities
double rand_double() { return (double)rand() / RAND_MAX; }
Vec3 random_in_unit_sphere() {
    Vec3 p;
    do {
        p = vec3(rand_double()*2 - 1, rand_double()*2 - 1, rand_double()*2 - 1);
    } while (dot(p, p) >= 1.0);
    return p;
}

// Sphere intersection
Hit sphere_intersect(Vec3 center, double radius, Ray ray, double t_min, double t_max) {
    Hit hit = {0};
    Vec3 oc = sub(ray.origin, center);
    double a = dot(ray.direction, ray.direction);
    double b = 2.0 * dot(oc, ray.direction);
    double c = dot(oc, oc) - radius * radius;
    double discriminant = b*b - 4*a*c;
    
    if (discriminant > 0) {
        double sqrt_d = sqrt(discriminant);
        double t = (-b - sqrt_d) / (2*a);
        if (t < t_min) t = (-b + sqrt_d) / (2*a);
        if (t >= t_min && t <= t_max) {
            hit.hit = 1;
            hit.t = t;
            hit.hit_point = add(ray.origin, mul(ray.direction, t));
            hit.normal = normalize(sub(hit.hit_point, center));
            return hit;
        }
    }
    return hit;
}

// Plane intersection (for checker floor)
Hit plane_intersect(Vec3 point, Vec3 normal, Ray ray, double t_min, double t_max) {
    Hit hit = {0};
    double denom = dot(normal, ray.direction);
    if (fabs(denom) > 1e-6) {
        Vec3 p0l = sub(point, ray.origin);
        double t = dot(p0l, normal) / denom;
        if (t >= t_min && t <= t_max) {
            hit.hit = 1;
            hit.t = t;
            hit.hit_point = add(ray.origin, mul(ray.direction, t));
            hit.normal = normal;
        }
    }
    return hit;
}

// Checkerboard pattern for floor
Vec3 checker_color(Vec3 point) {
    int x = (int)floor(point.x * 2.0);
    int z = (int)floor(point.z * 2.0);
    if ((x + z) % 2 == 0)
        return vec3(0.9, 0.9, 0.9);  // white tile
    else
        return vec3(0.3, 0.3, 0.5);  // dark blue-grey tile
}

// Starry sky background
Vec3 sky_color(Ray ray) {
    Vec3 dir = normalize(ray.direction);
    double t = 0.5 * (dir.y + 1.0);
    
    // Gradient from dark blue to black
    Vec3 sky_grad = add(vec3(0.02, 0.05, 0.15), mul(vec3(0.01, 0.02, 0.05), t));
    
    // Add stars using procedural noise based on direction
    double star_intensity = 0.0;
    // Simple star pattern using sin/cos of large numbers
    double x = fabs(fmod(dir.x * 487.0 + dir.z * 391.0, 1.0));
    double y = fabs(fmod(dir.y * 523.0 + dir.z * 317.0, 1.0));
    double star_rand = fmod(x * y * 1000.0, 1.0);
    
    if (star_rand > 0.995 && dir.y > -0.3) {
        star_intensity = 0.8 + rand_double() * 0.4;
    } else if (star_rand > 0.99 && dir.y > -0.2) {
        star_intensity = 0.4 + rand_double() * 0.3;
    }
    
    return add(sky_grad, vec3(star_intensity, star_intensity * 0.9, star_intensity));
}

// Reflection calculation
Vec3 reflect(Vec3 v, Vec3 n) {
    return sub(v, mul(n, 2.0 * dot(v, n)));
}

// Scene intersection (returns closest hit)
Hit scene_intersect(Ray ray, double t_min, double t_max) {
    Hit closest = {0};
    closest.t = t_max;
    closest.hit = 0;
    
    // Sphere at (0, 1.2, 0) radius 1.2
    Hit sphere_hit = sphere_intersect(vec3(0, 1.2, 0), 1.2, ray, t_min, t_max);
    if (sphere_hit.hit && sphere_hit.t < closest.t) {
        closest = sphere_hit;
        closest.normal = sphere_hit.normal;
    }
    
    // Floor plane at y = -0.2
    Hit floor_hit = plane_intersect(vec3(0, -0.2, 0), vec3(0, 1, 0), ray, t_min, t_max);
    if (floor_hit.hit && floor_hit.t < closest.t) {
        closest = floor_hit;
    }
    
    return closest;
}

// Ray tracing color function
Vec3 trace_ray(Ray ray, int depth) {
    if (depth >= MAX_BOUNCES) return vec3(0, 0, 0);
    
    Hit hit = scene_intersect(ray, 0.001, 1e6);
    
    if (!hit.hit) {
        return sky_color(ray);
    }
    
    // Hit the floor
    if (fabs(hit.normal.y - 1.0) < 0.001 && hit.hit_point.y < 0) {
        Vec3 floor_color = checker_color(hit.hit_point);
        
        // Add simple lighting
        Vec3 light_pos = vec3(5, 8, 3);
        Vec3 light_dir = normalize(sub(light_pos, hit.hit_point));
        double diff = fmax(0.2, dot(hit.normal, light_dir));
        
        // Ambient + diffuse
        Vec3 ambient = mul(floor_color, 0.25);
        Vec3 diffuse = mul(floor_color, diff * 0.9);
        
        return add(ambient, diffuse);
    }
    
    // Hit the shiny sphere
    Vec3 sphere_color = vec3(0.95, 0.92, 0.88);
    
    // Lighting calculation
    Vec3 light_pos = vec3(5, 8, 3);
    Vec3 light_dir = normalize(sub(light_pos, hit.hit_point));
    
    // Check shadow
    Ray shadow_ray = {hit.hit_point, light_dir};
    Hit shadow_hit = scene_intersect(shadow_ray, 0.01, length(sub(light_pos, hit.hit_point)));
    double shadow_intensity = shadow_hit.hit ? 0.35 : 1.0;
    
    // Diffuse lighting
    double diff = fmax(0.1, dot(hit.normal, light_dir)) * shadow_intensity;
    
    // Specular highlight (phong)
    Vec3 view_dir = normalize(mul(ray.direction, -1));
    Vec3 reflect_dir = reflect(mul(light_dir, -1), hit.normal);
    double spec = pow(fmax(0, dot(view_dir, reflect_dir)), 64);
    Vec3 specular = mul(vec3(1.0, 0.95, 0.9), spec * 0.9 * shadow_intensity);
    
    // Ambient + diffuse + specular
    Vec3 ambient = mul(sphere_color, 0.18);
    Vec3 diffuse = mul(sphere_color, diff * 0.85);
    
    Vec3 color = add(add(ambient, diffuse), specular);
    
    // Add reflection (shiny!)
    Ray reflect_ray = {hit.hit_point, reflect(ray.direction, hit.normal)};
    Vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
    color = add(color, mul(reflect_color, 0.55));
    
    // Add subtle rim lighting
    Vec3 rim_light = vec3(0.6, 0.4, 0.3);
    double rim = pow(1 - fmax(0, dot(view_dir, hit.normal)), 3);
    color = add(color, mul(rim_light, rim * 0.35));
    
    return color;
}

// Camera: generate ray for pixel (x, y)
Ray get_ray(int x, int y, double u, double v) {
    double aspect = (double)WIDTH / HEIGHT;
    double fov = M_PI / 3.0;  // 60 degrees
    
    double px = (2.0 * ((x + u) / WIDTH) - 1.0) * tan(fov / 2.0) * aspect;
    double py = (1.0 - 2.0 * ((y + v) / HEIGHT)) * tan(fov / 2.0);
    double pz = -1.0;
    
    Vec3 direction = normalize(vec3(px, py, pz));
    Ray ray = {vec3(0, 1.2, 3.5), direction};
    return ray;
}

int main() {
    printf("P3\n%d %d\n255\n", WIDTH, HEIGHT);
    
    srand(time(NULL));
    
    // Progress indicator
    fprintf(stderr, "Rendering retro CGI scene...\n");
    fprintf(stderr, "  Shiny sphere | Checker floor | Starry sky\n");
    
    for (int y = 0; y < HEIGHT; y++) {
        if (y % 50 == 0) {
            fprintf(stderr, "  Progress: %.1f%%\n", (double)y / HEIGHT * 100);
        }
        
        for (int x = 0; x < WIDTH; x++) {
            Vec3 color = {0, 0, 0};
            
            // Anti-aliasing with random subpixel samples
            for (int s = 0; s < SAMPLES; s++) {
                double u = (double)rand() / RAND_MAX;
                double v = (double)rand() / RAND_MAX;
                Ray ray = get_ray(x, y, u, v);
                Vec3 sample_color = trace_ray(ray, 0);
                color = add(color, sample_color);
            }
            
            // Average samples
            color = mul(color, 1.0 / SAMPLES);
            
            // Gamma correction
            color.x = pow(color.x, 1.0/2.2);
            color.y = pow(color.y, 1.0/2.2);
            color.z = pow(color.z, 1.0/2.2);
            
            // Clamp and output
            int r = (int)(fmin(1.0, fmax(0.0, color.x)) * 255);
            int g = (int)(fmin(1.0, fmax(0.0, color.y)) * 255);
            int b = (int)(fmin(1.0, fmax(0.0, color.z)) * 255);
            
            printf("%d %d %d\n", r, g, b);
        }
    }
    
    fprintf(stderr, "Complete! Image saved to stdout.\n");
    return 0;
}
