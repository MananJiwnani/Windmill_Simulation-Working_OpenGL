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

// ============================================================
//  WINDMILL FARM SCENE
//  GLTF node "prop"      (node 123) — spinning blade assembly
//  GLTF node "mill foot" (node 121) — static tower
//  Single texture: assets/textures/Custom_baseColor.png
// ============================================================

// ---------------------------------------------------------------
// Global state
// ---------------------------------------------------------------
float  windSpeedRef  = 1.5f;  // rotations per second
float  bladeRotation = 0.0f;  // accumulated degrees
double lastTime      = 0.0;

// Camera orbits around scene centre
float camRadius = 35.0f;
float camPitch  = 20.0f;   // degrees, clamped [-5, 89]
float camYaw    = 45.0f;   // degrees

// Approximate midpoint of the whole farm scene
const float sceneCX = -1.0f;
const float sceneCY =  3.0f;
const float sceneCZ =  0.0f;

double lastMouseX = 0.0, lastMouseY = 0.0;
int    isRotating = 0;

// GL / GLTF resources
GLuint*     gl_textures = NULL;
cgltf_data* model_data  = NULL;


// ---------------------------------------------------------------
// GLFW error callback
// ---------------------------------------------------------------
static void glfw_error_callback(int code, const char* desc)
{
    fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}


// ---------------------------------------------------------------
// Texture loader
// ---------------------------------------------------------------
GLuint load_texture(const char* path, int repeat)
{
    GLuint tex;
    glGenTextures(1, &tex);

    int w, h, ch;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    if (data) {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
        printf("Loaded texture : %s  (%dx%d  %dch)\n", path, w, h, ch);
    } else {
        printf("FAILED texture : %s\n", path);
    }
    return tex;
}


// ---------------------------------------------------------------
// GLFW callbacks
// (void)-cast every unused param to silence -Wunused-parameter
// ---------------------------------------------------------------
void mouse_button_callback(GLFWwindow* w, int b, int a, int mods)
{
    (void)mods;
    if (b == GLFW_MOUSE_BUTTON_LEFT) {
        isRotating = (a == GLFW_PRESS);
        glfwGetCursorPos(w, &lastMouseX, &lastMouseY);
    }
}

void mouse_callback(GLFWwindow* w, double x, double y)
{
    (void)w;
    if (isRotating) {
        camYaw   += (float)(x - lastMouseX) * 0.2f;
        camPitch += (float)(lastMouseY - y) * 0.2f;
        lastMouseX = x;
        lastMouseY = y;
        if (camPitch >  89.0f) camPitch =  89.0f;
        if (camPitch <  -5.0f) camPitch =  -5.0f;
    }
}

void scroll_callback(GLFWwindow* w, double dx, double dy)
{
    (void)w;
    (void)dx;
    camRadius -= (float)dy * 1.5f;
    if (camRadius <   5.0f) camRadius =   5.0f;
    if (camRadius > 200.0f) camRadius = 200.0f;
}

void key_callback(GLFWwindow* w, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_UP   || key == GLFW_KEY_W) windSpeedRef += 0.2f;
        if (key == GLFW_KEY_DOWN || key == GLFW_KEY_S) {
            windSpeedRef -= 0.2f;
            if (windSpeedRef < 0.0f) windSpeedRef = 0.0f;
        }
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
    }
}


// ---------------------------------------------------------------
// Draw one GLTF primitive (immediate mode)
// ---------------------------------------------------------------
void draw_primitive(cgltf_primitive* prim)
{
    if (!prim->indices) return;

    cgltf_accessor *pos = NULL, *uv = NULL, *norm = NULL;
    for (cgltf_size i = 0; i < prim->attributes_count; i++) {
        switch (prim->attributes[i].type) {
            case cgltf_attribute_type_position: pos  = prim->attributes[i].data; break;
            case cgltf_attribute_type_texcoord: uv   = prim->attributes[i].data; break;
            case cgltf_attribute_type_normal:   norm = prim->attributes[i].data; break;
            default: break;
        }
    }
    if (!pos) return;

    int has_tex = 0;
    if (prim->material && prim->material->has_pbr_metallic_roughness) {
        cgltf_texture* t =
            prim->material->pbr_metallic_roughness.base_color_texture.texture;
        if (t && t->image) {
            for (cgltf_size i = 0; i < model_data->images_count; i++) {
                if (&model_data->images[i] == t->image) {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, gl_textures[i]);
                    has_tex = 1;
                    break;
                }
            }
        }
    }

    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_TRIANGLES);
    for (cgltf_size i = 0; i < prim->indices->count; i++) {
        int idx = (int)cgltf_accessor_read_index(prim->indices, i);
        if (norm) { float n[3]; cgltf_accessor_read_float(norm, idx, n, 3); glNormal3fv(n); }
        if (uv)   { float u[2]; cgltf_accessor_read_float(uv,   idx, u, 2); glTexCoord2fv(u); }
        float p[3]; cgltf_accessor_read_float(pos, idx, p, 3); glVertex3fv(p);
    }
    glEnd();

    if (has_tex) glDisable(GL_TEXTURE_2D);
}


// ---------------------------------------------------------------
// Recursive node renderer
// "prop" node gets an extra glRotatef on its local Z — the GLTF
// baked quaternion already tilts the blades into the right plane.
// ---------------------------------------------------------------
void render_node(cgltf_node* node)
{
    glPushMatrix();

    mat4 local;
    cgltf_node_transform_local(node, (float*)local);
    glMultMatrixf((float*)local);

    if (node->name && strcasestr(node->name, "prop"))
        glRotatef(bladeRotation, 0.0f, 1.0f, 0.0f);

    if (node->mesh)
        for (cgltf_size i = 0; i < node->mesh->primitives_count; i++)
            draw_primitive(&node->mesh->primitives[i]);

    for (cgltf_size i = 0; i < node->children_count; i++)
        render_node(node->children[i]);

    glPopMatrix();
}


// ---------------------------------------------------------------
// Lighting (call once after context is current)
// ---------------------------------------------------------------
void setup_lighting(void)
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    float light_pos[] = { 20.0f, 40.0f, 20.0f, 1.0f };
    float ambient[]   = {  0.4f,  0.4f,  0.4f, 1.0f };
    float diffuse[]   = {  1.0f,  1.0f,  1.0f, 1.0f };
    float specular[]  = {  0.3f,  0.3f,  0.3f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
}


// ---------------------------------------------------------------
// main
// ---------------------------------------------------------------
int main(void)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }

    // Request a compatibility-profile context so legacy GL API
    // (glBegin/glEnd, glLightfv, etc.) works alongside GLEW.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    // Do NOT use GLFW_OPENGL_CORE_PROFILE — that strips legacy API.

    GLFWwindow* win = glfwCreateWindow(1280, 720,
                                       "Farm Windmill Sim", NULL, NULL);
    if (!win) {
        fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    glfwSetCursorPosCallback(win,   mouse_callback);
    glfwSetScrollCallback(win,      scroll_callback);
    glfwSetMouseButtonCallback(win, mouse_button_callback);
    glfwSetKeyCallback(win,         key_callback);

    // glewExperimental = GL_TRUE lets GLEW expose all extension entry-
    // points on compatibility contexts; without it glewInit() may fail
    // on some drivers even when the context is valid.
    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        fprintf(stderr, "glewInit failed: %s\n", glewGetErrorString(glewErr));
        glfwTerminate();
        return 1;
    }
    glGetError();   // clear the spurious GL_INVALID_ENUM glewInit can leave

    printf("OpenGL  : %s\n", glGetString(GL_VERSION));
    printf("GLSL    : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    setup_lighting();

    // --- Load GLTF ---
    // Extract the zip so assets/ looks like:
    //   assets/scene.gltf
    //   assets/scene.bin
    //   assets/textures/Custom_baseColor.png
    cgltf_options opt = {0};
    cgltf_result  res = cgltf_parse_file(&opt, "assets/scene.gltf", &model_data);
    if (res == cgltf_result_success) {
        cgltf_load_buffers(&opt, model_data, "assets/scene.gltf");
        gl_textures = malloc(sizeof(GLuint) * model_data->images_count);
        for (cgltf_size i = 0; i < model_data->images_count; i++) {
            char path[512];
            snprintf(path, sizeof(path), "assets/%s", model_data->images[i].uri);
            gl_textures[i] = load_texture(path, 0);
        }
        printf("GLTF loaded : %d meshes, %d images\n",
               (int)model_data->meshes_count,
               (int)model_data->images_count);
    } else {
        fprintf(stderr, "Failed to load assets/scene.gltf (error %d)\n", res);
    }

    // --- Main loop ---
    lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(win)) {

        double t  = glfwGetTime();
        float  dt = (float)(t - lastTime);
        lastTime  = t;

        bladeRotation += windSpeedRef * 360.0f * dt;
        if (bladeRotation >= 360.0f) bladeRotation -= 360.0f;

        glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int fbW, fbH;
        glfwGetFramebufferSize(win, &fbW, &fbH);
        if (fbH == 0) fbH = 1;
        glViewport(0, 0, fbW, fbH);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (double)fbW / (double)fbH, 0.1, 500.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        float pitchRad = glm_rad(camPitch);
        float yawRad   = glm_rad(camYaw);
        float eyeX = sceneCX + camRadius * cosf(pitchRad) * sinf(yawRad);
        float eyeY = sceneCY + camRadius * sinf(pitchRad);
        float eyeZ = sceneCZ + camRadius * cosf(pitchRad) * cosf(yawRad);

        gluLookAt(eyeX, eyeY, eyeZ,
                  sceneCX, sceneCY, sceneCZ,
                  0.0, 1.0, 0.0);

        // Re-submit light pos every frame so it stays world-fixed
        float light_pos[] = { 20.0f, 40.0f, 20.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

        if (model_data)
            for (cgltf_size i = 0; i < model_data->scenes[0].nodes_count; i++)
                render_node(model_data->scenes[0].nodes[i]);

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    if (gl_textures) free(gl_textures);
    if (model_data)  cgltf_free(model_data);
    glfwTerminate();
    return 0;
}
