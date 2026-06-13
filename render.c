// retro_cgi_interactive.cpp
// Compile: g++ -o retro_cgi.exe retro_cgi_interactive.cpp -lglfw3 -lopengl32 -lgdi32 -lm
// Requires: GLFW, GLEW (or glad), and stb_image.h

#include <iostream>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Math structures
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };

Vec3 vec3(float x, float y, float z) { return {x, y, z}; }
Vec4 vec4(float x, float y, float z, float w) { return {x, y, z, w}; }

// Camera controls
struct Camera {
    Vec3 pos = vec3(0, 2, 6);
    Vec3 front = vec3(0, 0, -1);
    Vec3 up = vec3(0, 1, 0);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float lastX = 400, lastY = 300;
    bool firstMouse = true;
    float speed = 5.0f;
    float sensitivity = 0.1f;
} cam;

// Shader sources
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform sampler2D texture1;
uniform float time;
uniform int isShiny;

void main() {
    vec3 objectColor = texture(texture1, TexCoord).rgb;
    
    // Ambient
    float ambientStrength = 0.25;
    vec3 ambient = ambientStrength * objectColor;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * objectColor;
    
    // Specular (shiny!)
    float specularStrength = isShiny == 1 ? 0.9 : 0.2;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), isShiny == 1 ? 64 : 8);
    vec3 specular = specularStrength * spec * vec3(1.0, 0.95, 0.9);
    
    // Rim lighting for shiny sphere
    vec3 rimColor = vec3(0.6, 0.4, 0.3);
    float rim = 1.0 - max(dot(viewDir, norm), 0.0);
    rim = pow(rim, 2.5) * 0.5;
    vec3 rimLight = rim * rimColor;
    
    vec3 result = ambient + diffuse + specular + rimLight;
    FragColor = vec4(result, 1.0);
}
)";

// Create checkerboard texture
GLuint createCheckerTexture() {
    int width = 512, height = 512;
    unsigned char* data = (unsigned char*)malloc(width * height * 3);
    
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            int x = (i / 64) % 2;
            int y = (j / 64) % 2;
            unsigned char r, g, b;
            if ((x + y) % 2 == 0) {
                r = 230; g = 220; b = 200; // light tile
            } else {
                r = 80; g = 70; b = 110;   // dark tile
            }
            data[(j * width + i) * 3 + 0] = r;
            data[(j * width + i) * 3 + 1] = g;
            data[(j * width + i) * 3 + 2] = b;
        }
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    free(data);
    return texture;
}

// Create shiny environment texture for sphere
GLuint createEnvTexture() {
    int width = 512, height = 512;
    unsigned char* data = (unsigned char*)malloc(width * height * 3);
    
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            float u = (float)i / width;
            float v = (float)j / height;
            // Fake environment map: gradient + stars
            unsigned char r = (unsigned char)(150 + 50 * sin(u * 20) * cos(v * 20));
            unsigned char g = (unsigned char)(130 + 70 * sin(u * 15 + 2) * sin(v * 18));
            unsigned char b = (unsigned char)(200 + 55 * cos(u * 25) * sin(v * 22));
            
            // Add some star-like specs
            if ((int)(u * 50) % 13 == 7 && (int)(v * 50) % 17 == 5) {
                r = 255; g = 255; b = 200;
            }
            
            data[(j * width + i) * 3 + 0] = r;
            data[(j * width + i) * 3 + 1] = g;
            data[(j * width + i) * 3 + 2] = b;
        }
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    free(data);
    return texture;
}

// Create star field background (skybox)
GLuint createStarTexture() {
    int width = 1024, height = 1024;
    unsigned char* data = (unsigned char*)malloc(width * height * 3);
    
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            // Deep space gradient
            float v = (float)j / height;
            unsigned char r = (unsigned char)(10 + 20 * v);
            unsigned char g = (unsigned char)(5 + 15 * v);
            unsigned char b = (unsigned char)(30 + 40 * v);
            
            // Random stars
            if ((rand() % 1000) > 995) {
                r = 255; g = 255; b = 255;
            } else if ((rand() % 1000) > 998) {
                r = 255; g = 200; b = 150;
            }
            
            data[(j * width + i) * 3 + 0] = r;
            data[(j * width + i) * 3 + 1] = g;
            data[(j * width + i) * 3 + 2] = b;
        }
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    free(data);
    return texture;
}

// Build sphere mesh (normals + tex coords)
void generateSphere(std::vector<float>& vertices, std::vector<unsigned int>& indices, float radius, int rings, int sectors) {
    float const R = 1.0f / (float)(rings - 1);
    float const S = 1.0f / (float)(sectors - 1);
    
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < sectors; s++) {
            float y = sin(-M_PI_2 + M_PI * r * R);
            float x = cos(2 * M_PI * s * S) * sin(M_PI * r * R);
            float z = sin(2 * M_PI * s * S) * sin(M_PI * r * R);
            
            vertices.push_back(x * radius);
            vertices.push_back(y * radius);
            vertices.push_back(z * radius);
            
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            
            vertices.push_back(s * S);
            vertices.push_back(r * R);
        }
    }
    
    for (int r = 0; r < rings - 1; r++) {
        for (int s = 0; s < sectors - 1; s++) {
            indices.push_back(r * sectors + s);
            indices.push_back((r + 1) * sectors + s);
            indices.push_back((r + 1) * sectors + (s + 1));
            
            indices.push_back(r * sectors + s);
            indices.push_back((r + 1) * sectors + (s + 1));
            indices.push_back(r * sectors + (s + 1));
        }
    }
}

// Build floor plane
void generateFloor(std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    float size = 15.0f;
    float y = -0.2f;
    
    vertices = {
        -size, y, -size,   0, 1, 0,   0, 0,
         size, y, -size,   0, 1, 0,   size, 0,
         size, y,  size,   0, 1, 0,   size, size,
        -size, y,  size,   0, 1, 0,   0, size
    };
    
    indices = {0, 1, 2,  0, 2, 3};
}

// Compile shader
GLuint compileShader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "Shader compilation failed: " << infoLog << std::endl;
    }
    return shader;
}

GLuint createShaderProgram() {
    GLuint vertexShader = compileShader(vertexShaderSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER);
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cout << "Program linking failed: " << infoLog << std::endl;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

// Mouse callback
void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (cam.firstMouse) {
        cam.lastX = xpos;
        cam.lastY = ypos;
        cam.firstMouse = false;
    }
    
    float xoffset = xpos - cam.lastX;
    float yoffset = cam.lastY - ypos;
    cam.lastX = xpos;
    cam.lastY = ypos;
    
    xoffset *= cam.sensitivity;
    yoffset *= cam.sensitivity;
    
    cam.yaw += xoffset;
    cam.pitch += yoffset;
    
    if (cam.pitch > 89.0f) cam.pitch = 89.0f;
    if (cam.pitch < -89.0f) cam.pitch = -89.0f;
    
    cam.front.x = cos(cam.yaw * M_PI / 180) * cos(cam.pitch * M_PI / 180);
    cam.front.y = sin(cam.pitch * M_PI / 180);
    cam.front.z = sin(cam.yaw * M_PI / 180) * cos(cam.pitch * M_PI / 180);
    cam.front = vec3(cam.front.x, cam.front.y, cam.front.z);
}

// Keyboard input
void processInput(GLFWwindow* window, float deltaTime) {
    float speed = cam.speed * deltaTime;
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cam.pos = vec3(cam.pos.x + cam.front.x * speed, cam.pos.y + cam.front.y * speed, cam.pos.z + cam.front.z * speed);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cam.pos = vec3(cam.pos.x - cam.front.x * speed, cam.pos.y - cam.front.y * speed, cam.pos.z - cam.front.z * speed);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        Vec3 right = {cam.front.z, 0, -cam.front.x};
        cam.pos = vec3(cam.pos.x - right.x * speed, cam.pos.y, cam.pos.z - right.z * speed);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        Vec3 right = {cam.front.z, 0, -cam.front.x};
        cam.pos = vec3(cam.pos.x + right.x * speed, cam.pos.y, cam.pos.z + right.z * speed);
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cam.pos.y += speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cam.pos.y -= speed;
}

int main() {
    srand(time(NULL));
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    GLFWwindow* window = glfwCreateWindow(1024, 768, "Retro CGI - Shiny Sphere | Checker Floor | Starry Sky", NULL, NULL);
    if (!window) {
        std::cout << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Create textures
    GLuint checkerTex = createCheckerTexture();
    GLuint envTex = createEnvTexture();
    GLuint starTex = createStarTexture();
    
    // Create sphere
    std::vector<float> sphereVerts;
    std::vector<unsigned int> sphereIndices;
    generateSphere(sphereVerts, sphereIndices, 1.2f, 64, 64);
    
    GLuint sphereVAO, sphereVBO, sphereEBO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glGenBuffers(1, &sphereEBO);
    
    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVerts.size() * sizeof(float), sphereVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereIndices.size() * sizeof(unsigned int), sphereIndices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Create floor
    std::vector<float> floorVerts;
    std::vector<unsigned int> floorIndices;
    generateFloor(floorVerts, floorIndices);
    
    GLuint floorVAO, floorVBO, floorEBO;
    glGenVertexArrays(1, &floorVAO);
    glGenBuffers(1, &floorVBO);
    glGenBuffers(1, &floorEBO);
    
    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER, floorVerts.size() * sizeof(float), floorVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, floorIndices.size() * sizeof(unsigned int), floorIndices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Create shader program
    GLuint shaderProgram = createShaderProgram();
    
    // Set up projection
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1024.0f / 768.0f, 0.1f, 100.0f);
    
    float lastFrame = 0.0f;
    float time = 0.0f;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  RETRO CGI INTERACTIVE DEMO" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Controls:" << std::endl;
    std::cout << "    WASD  - Move around" << std::endl;
    std::cout << "    Mouse  - Look around" << std::endl;
    std::cout << "    SPACE  - Fly up" << std::endl;
    std::cout << "    L-SHIFT - Fly down" << std::endl;
    std::cout << "    ESC    - Exit" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        time += deltaTime;
        
        processInput(window, deltaTime);
        
        glClearColor(0.02f, 0.02f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shaderProgram);
        
        // Set uniforms
        GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
        GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
        GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
        GLint lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
        GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
        GLint timeLoc = glGetUniformLocation(shaderProgram, "time");
        GLint isShinyLoc = glGetUniformLocation(shaderProgram, "isShiny");
        
        // Animate light position
        Vec3 lightPos = vec3(5 * sin(time * 0.5f), 6 + sin(time * 0.7f) * 2, 5 * cos(time * 0.5f));
        
        // View matrix from camera
        Vec3 center = vec3(cam.pos.x + cam.front.x, cam.pos.y + cam.front.y, cam.pos.z + cam.front.z);
        glm::mat4 view = glm::lookAt(glm::vec3(cam.pos.x, cam.pos.y, cam.pos.z), 
                                      glm::vec3(center.x, center.y, center.z), 
                                      glm::vec3(0, 1, 0));
        
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);
        glUniform3f(lightPosLoc, lightPos.x, lightPos.y, lightPos.z);
        glUniform3f(viewPosLoc, cam.pos.x, cam.pos.y, cam.pos.z);
        glUniform1f(timeLoc, time);
        
        // Draw sphere (shiny)
        glUniform1i(isShinyLoc, 1);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0, 1.2f, 0));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, envTex);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        
        // Draw floor (checker)
        glUniform1i(isShinyLoc, 0);
        model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
        glBindTexture(GL_TEXTURE_2D, checkerTex);
        glBindVertexArray(floorVAO);
        glDrawElements(GL_TRIANGLES, floorIndices.size(), GL_UNSIGNED_INT, 0);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    }
    
    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteVertexArrays(1, &floorVAO);
    glDeleteBuffers(1, &floorVBO);
    glDeleteProgram(shaderProgram);
    
    glfwTerminate();
    return 0;
}
