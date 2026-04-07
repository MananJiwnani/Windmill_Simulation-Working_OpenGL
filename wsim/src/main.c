#define _GNU_SOURCE
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <cglm/cglm.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --- Global Variables ---
float windSpeedRef = 12.0f;
float bladeRotation = 0.0f;
double lastTime = 0.0;

float camRadius = 200.0f, camPitch = 25.0f, camYaw = 45.0f;
double lastMouseX, lastMouseY;
int isRotating = 0;

GLuint* gl_textures = NULL;
GLuint grassTexID;
cgltf_data* model_data = NULL;

// --- Helper: Load Texture ---
GLuint load_texture(const char* path, int repeat) {
    GLuint tex;
    glGenTextures(1, &tex);
    int w, h, ch;
    // Force stbi to load 3 or 4 channels depending on what we need
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    if (data) {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, tex);
        
        // Use GL_MODULATE so lighting affects the texture color
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        stbi_image_free(data);
    } else {
        printf("FAILED to load texture: %s\n", path);
    }
    return tex;
}

// --- Callbacks ---
void mouse_button_callback(GLFWwindow* w, int b, int a, int m) {
    if (b == GLFW_MOUSE_BUTTON_LEFT) { isRotating = (a == GLFW_PRESS); glfwGetCursorPos(w, &lastMouseX, &lastMouseY); }
}
void mouse_callback(GLFWwindow* w, double x, double y) {
    if (isRotating) {
        camYaw += (float)(x - lastMouseX) * 0.2f; camPitch += (float)(lastMouseY - y) * 0.2f;
        lastMouseX = x; lastMouseY = y;
        if (camPitch > 89.0f) camPitch = 89.0f; if (camPitch < -5.0f) camPitch = -5.0f;
    }
}
void scroll_callback(GLFWwindow* w, double x, double y) { camRadius -= (float)y * 8.0f; if (camRadius < 30.0f) camRadius = 30.0f; }

// --- Rendering ---
void draw_primitive(cgltf_primitive* prim) {
    cgltf_accessor *pos = NULL, *uv = NULL, *norm = NULL;
    for (int i = 0; i < prim->attributes_count; i++) {
        if (prim->attributes[i].type == cgltf_attribute_type_position) pos = prim->attributes[i].data;
        if (prim->attributes[i].type == cgltf_attribute_type_texcoord) uv = prim->attributes[i].data;
        if (prim->attributes[i].type == cgltf_attribute_type_normal) norm = prim->attributes[i].data;
    }

    if (prim->material && prim->material->has_pbr_metallic_roughness) {
        cgltf_texture* t = prim->material->pbr_metallic_roughness.base_color_texture.texture;
        if (t && t->image) {
            glEnable(GL_TEXTURE_2D);
            for(int i=0; i<model_data->images_count; i++) 
                if(&model_data->images[i] == t->image) glBindTexture(GL_TEXTURE_2D, gl_textures[i]);
        }
    }

    glBegin(GL_TRIANGLES);
    // Set to white so texture color is unmodified
    glColor3f(1.0f, 1.0f, 1.0f); 
    for (int i = 0; i < prim->indices->count; i++) {
        int idx = cgltf_accessor_read_index(prim->indices, i);
        if (norm) { float n[3]; cgltf_accessor_read_float(norm, idx, n, 3); glNormal3fv(n); }
        if (uv) { float u[2]; cgltf_accessor_read_float(uv, idx, u, 2); glTexCoord2fv(u); }
        float p[3]; cgltf_accessor_read_float(pos, idx, p, 3);
        glVertex3fv(p);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void render_node(cgltf_node* node) {
    glPushMatrix();
    mat4 m; cgltf_node_transform_local(node, (float*)m); glMultMatrixf((float*)m);
    if (node->name && (strcasestr(node->name, "blade") || strcasestr(node->name, "rotor")))
        glRotatef(bladeRotation, 0, 0, 1);
    if (node->mesh) for (int i = 0; i < node->mesh->primitives_count; i++) draw_primitive(&node->mesh->primitives[i]);
    for (int i = 0; i < node->children_count; i++) render_node(node->children[i]);
    glPopMatrix();
}

void setup_lighting() {
    glEnable(GL_LIGHTING); 
    glEnable(GL_LIGHT0); 
    glEnable(GL_COLOR_MATERIAL); // CRITICAL: Makes OpenGL listen to glColor3f
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    float light_pos[] = { 200.0f, 500.0f, 200.0f, 1.0f };
    float ambient[]   = { 0.5f, 0.5f, 0.5f, 1.0f }; // Brighter ambient for better colors
    float diffuse[]   = { 1.0f, 1.0f, 1.0f, 1.0f };
    
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
}

int main() {
    glfwInit();
    GLFWwindow* win = glfwCreateWindow(1280, 720, "Pro Windmill Sim", NULL, NULL);
    glfwMakeContextCurrent(win);
    glfwSetCursorPosCallback(win, mouse_callback);
    glfwSetScrollCallback(win, scroll_callback);
    glfwSetMouseButtonCallback(win, mouse_button_callback);
    glewInit();
    glEnable(GL_DEPTH_TEST);
    setup_lighting();

    // Load Model and Model Textures
    cgltf_options opt = {0};
    if (cgltf_parse_file(&opt, "assets/scene.gltf", &model_data) == cgltf_result_success) {
        cgltf_load_buffers(&opt, model_data, "assets/scene.gltf");
        gl_textures = malloc(sizeof(GLuint) * model_data->images_count);
        for (int i = 0; i < model_data->images_count; i++) {
            char p[512]; snprintf(p, 512, "assets/%s", model_data->images[i].uri);
            gl_textures[i] = load_texture(p, 0);
        }
    }
    
    // Load Grass Texture
    grassTexID = load_texture("assets/textures/grass1.jpg", 1);

    while (!glfwWindowShouldClose(win)) {
        double t = glfwGetTime(); float dt = (float)(t - lastTime); lastTime = t;
        bladeRotation += windSpeedRef * 40.0f * dt;

        glClearColor(0.5f, 0.8f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective(45, 1280.0/720.0, 1, 5000);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        float cX = camRadius * cosf(glm_rad(camPitch)) * sinf(glm_rad(camYaw));
        float cY = camRadius * sinf(glm_rad(camPitch));
        float cZ = camRadius * cosf(glm_rad(camPitch)) * cosf(glm_rad(camYaw));
        gluLookAt(cX, cY + 30, cZ, 0, 40, 0, 0, 1, 0);

        // --- Render Landscape ---
// --- Render Landscape ---
        // Disable lighting so the grass texture renders in its true colors.
        // GL_MODULATE multiplies texture by the lit material color,
        // which desaturates it to grey/black without this.
        glDisable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D); 
        glBindTexture(GL_TEXTURE_2D, grassTexID);
        
        glBegin(GL_QUADS); 
        glNormal3f(0, 1, 0); 
        glColor3f(1.0f, 1.0f, 1.0f);
        
        glTexCoord2f(0,   0);   glVertex3f(-1000, 0, -1000);
        glTexCoord2f(100, 0);   glVertex3f( 1000, 0, -1000);
        glTexCoord2f(100, 100); glVertex3f( 1000, 0,  1000);
        glTexCoord2f(0,   100); glVertex3f(-1000, 0,  1000);
        glEnd(); 
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_LIGHTING); // Re-enable lighting for the windmill model

        // --- Render Windmill ---
        if (model_data) for (int i = 0; i < model_data->scenes[0].nodes_count; i++) render_node(model_data->scenes[0].nodes[i]);

        glfwSwapBuffers(win); glfwPollEvents();
    }
    
    if (gl_textures) free(gl_textures);
    if (model_data) cgltf_free(model_data);
    glfwTerminate(); 
    return 0;
}
