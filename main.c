#define _GNU_SOURCE
#include <GL/glew.h>
#include <GL/glut.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <cglm/cglm.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ---------------------------------------------------------------
// Physics & wind state (ported from GLUT windmill)
// ---------------------------------------------------------------
double wing_z = 0.0;
double wind_y = 0.0;
double wing_speed = 0.0;
double wind_acc_factor = 0.8;
double turbine_factor = 0.005;
double progstep = 0.0;
double progstep_acc = 0.0005;
double r_step = 5.0;
double torqueFact = 0.00002;

double wind_speed_target = 0.0;
double wind_angle_target = 0.0;
int isRandom = 0;

double lastTime = 0.0;

// Camera
float camRadius = 35.0f;
float camPitch = 20.0f;
float camYaw = 45.0f;

const float sceneCX = -1.0f;
const float sceneCY = 3.0f;
const float sceneCZ = 0.0f;

double lastMouseX = 0.0, lastMouseY = 0.0;
int isRotating = 0;

// GL / GLTF resources
GLuint *gl_textures = NULL;
cgltf_data *model_data = NULL;

// Form shadow intensity control
float shadow_intensity = 0.9f;  // Very dark shadows for dramatic form

// Light properties - fixed 5 o'clock sun position
// 5 o'clock = 17:00, which is 11 hours after 6 AM
// Hour angle = (17 - 6) * 15 = 165°
double sun_azimuth = 165.0;  // 5 o'clock position (165° from north)
double sun_elevation = 40.0; // Afternoon elevation angle

// Current light direction (will be updated once at startup)
float light_dir[3]; // Normalized direction TO the light
float light_pos[3]; // Position far away

// ---------------------------------------------------------------
// GLFW error callback
// ---------------------------------------------------------------
static void glfw_error_callback(int code, const char *desc)
{
    fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

// ---------------------------------------------------------------
// Texture loader
// ---------------------------------------------------------------
GLuint load_texture(const char *path, int repeat)
{
    GLuint tex;
    glGenTextures(1, &tex);

    int w, h, ch;
    unsigned char *data = stbi_load(path, &w, &h, &ch, 0);
    if (data)
    {
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
    }
    else
    {
        printf("FAILED texture : %s\n", path);
    }
    return tex;
}

// ---------------------------------------------------------------
// GLFW callbacks
// ---------------------------------------------------------------
void mouse_button_callback(GLFWwindow *w, int b, int a, int mods)
{
    (void)mods;
    if (b == GLFW_MOUSE_BUTTON_LEFT)
    {
        isRotating = (a == GLFW_PRESS);
        glfwGetCursorPos(w, &lastMouseX, &lastMouseY);
    }
}

void mouse_callback(GLFWwindow *w, double x, double y)
{
    (void)w;
    if (isRotating)
    {
        camYaw -= (float)(x - lastMouseX) * 0.2f;
        camPitch -= (float)(lastMouseY - y) * 0.2f;
        lastMouseX = x;
        lastMouseY = y;
        if (camPitch > 89.0f)
            camPitch = 89.0f;
        if (camPitch < -5.0f)
            camPitch = -5.0f;
    }
}

void scroll_callback(GLFWwindow *w, double dx, double dy)
{
    (void)w;
    (void)dx;
    camRadius -= (float)dy * 1.5f;
    if (camRadius < 5.0f)
        camRadius = 5.0f;
    if (camRadius > 200.0f)
        camRadius = 200.0f;
}

void key_callback(GLFWwindow *w, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        // Wind speed
        if (key == GLFW_KEY_UP)
        {
            if (progstep < 0.3)
                progstep += progstep_acc;
        }
        if (key == GLFW_KEY_DOWN)
        {
            if (progstep > -0.3)
                progstep -= progstep_acc;
        }
        // Wind direction
        if (key == GLFW_KEY_LEFT)
            wind_y -= r_step;
        if (key == GLFW_KEY_RIGHT)
            wind_y += r_step;
        // Camera orbit
        if (key == GLFW_KEY_A)
            camYaw -= (float)r_step;
        if (key == GLFW_KEY_D)
            camYaw += (float)r_step;
        // Camera pitch
        if (key == GLFW_KEY_W)
        {
            camPitch += 2.0f;
            if (camPitch > 89.0f)
                camPitch = 89.0f;
        }
        if (key == GLFW_KEY_S)
        {
            camPitch -= 2.0f;
            if (camPitch < -5.0f)
                camPitch = -5.0f;
        }
        // Toggle random wind
        if (key == GLFW_KEY_R)
        {
            isRandom = 1 - isRandom;
            wind_speed_target = (double)(rand() % 50);
            wind_angle_target = (double)(rand() % 360);
        }
        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(w, GLFW_TRUE);
    }
}

// ---------------------------------------------------------------
// Physics update (ported from GLUT rotate() idle func)
// ---------------------------------------------------------------
void update_physics(void)
{
    if (isRandom)
    {
        if (rand() % 1000 == 0)
        {
            wind_speed_target = (double)(rand() % 50);
            wind_angle_target = (double)(rand() % 360);
        }
        progstep += (0.008 * (wind_speed_target - progstep * 1000.0)) / 1000.0;
        wind_y += 0.002 * (wind_angle_target - wind_y);
    }

    double acc = progstep * cos(wind_y / 180.0 * M_PI) * wind_acc_factor - wing_speed * turbine_factor;
    wing_speed += acc;
    wing_z += wing_speed;

    if (wing_z >= 360.0)
        wing_z -= 360.0;
    if (wing_z < 0.0)
        wing_z += 360.0;
}

// ---------------------------------------------------------------
// Helper: draw a rounded rectangle outline
// ---------------------------------------------------------------
void draw_rounded_rect_outline(float x, float y, float w, float h, float radius, float r, float g, float b, float a)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, a);

    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= 10; i++)
    {
        float angle = (float)i / 10.0f * M_PI * 2.0f;
        glVertex2f(x + w / 2 + (w / 2 - radius) * cosf(angle),
                   y + h / 2 + (h / 2 - radius) * sinf(angle));
    }
    glEnd();

    glDisable(GL_BLEND);
}

// ---------------------------------------------------------------
// HUD rendering (ortho overlay) - Modern Dashboard
// ---------------------------------------------------------------
void render_hud(int fbW, int fbH)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, fbW, 0, fbH, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

    char hud[256];
    float panel_x = 15.0f;
    float panel_y = (float)fbH - 280.0f;
    float panel_w = 480.0f;
    float panel_h = 265.0f;

    // Modern dark background with gradient effect
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Main panel background - modern dark
    glColor4f(0.08f, 0.1f, 0.15f, 0.85f);
    glBegin(GL_QUADS);
    glVertex2f(panel_x, panel_y);
    glVertex2f(panel_x + panel_w, panel_y);
    glVertex2f(panel_x + panel_w, panel_y + panel_h);
    glVertex2f(panel_x, panel_y + panel_h);
    glEnd();

    // Accent border all sides (3px)
    glColor4f(0.0f, 0.7f, 1.0f, 0.6f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(panel_x + 1.5f, panel_y + 1.5f);
    glVertex2f(panel_x + panel_w - 1.5f, panel_y + 1.5f);
    glVertex2f(panel_x + panel_w - 1.5f, panel_y + panel_h - 1.5f);
    glVertex2f(panel_x + 1.5f, panel_y + panel_h - 1.5f);
    glEnd();

    glDisable(GL_BLEND);

    float x = panel_x + 20.0f;
    float y = panel_y + panel_h - 22.0f;
    float line_spacing = 24.0f;

    // Title - Modern styling
    glColor3f(0.0f, 0.8f, 1.0f);
    glRasterPos2f(x, y);
    const char *title = "WINDMILL SIMULATION";
    for (const char *c = title; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    y -= line_spacing + 4.0f;

    // Section divider
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.7f, 1.0f, 0.3f);
    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x + 440.0f, y);
    glEnd();
    glDisable(GL_BLEND);

    y -= 8.0f;

    // Wind speed
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Speed:");
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    glColor3f(0.2f, 1.0f, 0.8f);
    glRasterPos2f(x + 90.0f, y);
    snprintf(hud, sizeof(hud), "%.2f km/hr", 1000.0 * fabs(progstep));
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);

    y -= line_spacing;

    // Wind direction
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Direction:");
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    glColor3f(0.2f, 1.0f, 0.8f);
    glRasterPos2f(x + 110.0f, y);
    snprintf(hud, sizeof(hud), "%.0f°", wind_y);
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);

    y -= line_spacing + 3.0f;

    // TURBINE METRICS divider
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 0.5f, 0.0f, 0.2f);
    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x + 440.0f, y);
    glEnd();
    glDisable(GL_BLEND);

    y -= 8.0f;

    // Blade speed
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Blade Speed:");
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    glColor3f(1.0f, 0.6f, 0.2f);
    glRasterPos2f(x + 120.0f, y);
    snprintf(hud, sizeof(hud), "%.1f°/fr", wing_speed);
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);

    y -= line_spacing;

    // Power output
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Output:");
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    double power = 1000.0 * fabs(torqueFact * wing_speed * wing_speed);
    glColor3f(0.2f, 1.0f, 0.3f);
    glRasterPos2f(x + 90.0f, y);
    snprintf(hud, sizeof(hud), "%.4f MW", power);
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);

    y -= line_spacing + 3.0f;

    // STATUS SECTION divider
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.8f, 0.5f, 0.2f, 0.2f);
    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x + 440.0f, y);
    glEnd();
    glDisable(GL_BLEND);

    y -= 8.0f;

    // Status indicator
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Status:");
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    if (isRandom)
    {
        glColor3f(0.2f, 1.0f, 0.5f);
        glRasterPos2f(x + 90.0f, y);
        const char *status = "● RANDOM WIND";
        for (const char *c = status; *c; c++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
    }
    else
    {
        glColor3f(1.0f, 0.9f, 0.2f);
        glRasterPos2f(x + 90.0f, y);
        const char *status = "● MANUAL CONTROL";
        for (const char *c = status; *c; c++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
    }

    y -= line_spacing + 4.0f;
    
    // Footer text
    glColor3f(0.5f, 0.6f, 0.7f);
    glRasterPos2f(x, y);
    const char *footer = "[ Mouse Drag: Orbit  |  Scroll: Zoom ]";
    for (const char *c = footer; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);

    glEnable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// ---------------------------------------------------------------
// Draw one GLTF primitive (immediate mode)
// ---------------------------------------------------------------
void draw_primitive(cgltf_primitive *prim)
{
    if (!prim->indices)
        return;

    cgltf_accessor *pos = NULL, *uv = NULL, *norm = NULL;
    for (cgltf_size i = 0; i < prim->attributes_count; i++)
    {
        switch (prim->attributes[i].type)
        {
        case cgltf_attribute_type_position:
            pos = prim->attributes[i].data;
            break;
        case cgltf_attribute_type_texcoord:
            uv = prim->attributes[i].data;
            break;
        case cgltf_attribute_type_normal:
            norm = prim->attributes[i].data;
            break;
        default:
            break;
        }
    }
    if (!pos)
        return;

    int has_tex = 0;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // Check for texture and color from material
    if (prim->material && prim->material->has_pbr_metallic_roughness)
    {
        // Try to apply baseColorTexture
        cgltf_texture *t =
            prim->material->pbr_metallic_roughness.base_color_texture.texture;
        if (t && t->image)
        {
            for (cgltf_size i = 0; i < model_data->images_count; i++)
            {
                if (&model_data->images[i] == t->image)
                {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, gl_textures[i]);
                    has_tex = 1;
                    break;
                }
            }
        }

        // Apply baseColorFactor if present (even if texture is used, for modulation)
        float *bcf = prim->material->pbr_metallic_roughness.base_color_factor;
        if (bcf)
        {
            for (int j = 0; j < 4; j++)
                color[j] = bcf[j];
        }
    }

    glColor4fv(color);

    glBegin(GL_TRIANGLES);
    for (cgltf_size i = 0; i < prim->indices->count; i++)
    {
        int idx = (int)cgltf_accessor_read_index(prim->indices, i);
        if (norm)
        {
            float n[3];
            cgltf_accessor_read_float(norm, idx, n, 3);
            glNormal3fv(n);
        }
        if (uv)
        {
            float u[2];
            cgltf_accessor_read_float(uv, idx, u, 2);
            glTexCoord2fv(u);
        }
        float p[3];
        cgltf_accessor_read_float(pos, idx, p, 3);
        glVertex3fv(p);
    }
    glEnd();

    if (has_tex)
        glDisable(GL_TEXTURE_2D);
}

// ---------------------------------------------------------------
// Recursive node renderer
// ---------------------------------------------------------------
void render_node(cgltf_node *node)
{
    glPushMatrix();

    mat4 local;
    cgltf_node_transform_local(node, (float *)local);
    glMultMatrixf((float *)local);

    if (node->name && strcasestr(node->name, "prop"))
        glRotatef((float)wing_z, 0.0f, 1.0f, 0.0f);

    if (node->mesh)
        for (cgltf_size i = 0; i < node->mesh->primitives_count; i++)
            draw_primitive(&node->mesh->primitives[i]);

    for (cgltf_size i = 0; i < node->children_count; i++)
        render_node(node->children[i]);

    glPopMatrix();
}

// ---------------------------------------------------------------
// Sun position initialization (fixed 5 o'clock)
// ---------------------------------------------------------------
void init_sun_position(void)
{
    // Convert hour angle and elevation to radians
    float azimuth = glm_rad((float)sun_azimuth);
    float elev_rad = glm_rad((float)sun_elevation);

    // Calculate light direction (FROM sun TO scene)
    light_dir[0] = sinf(azimuth) * cosf(elev_rad);
    light_dir[1] = sinf(elev_rad);
    light_dir[2] = cosf(azimuth) * cosf(elev_rad);

    // Calculate light position (far away in opposite direction)
    light_pos[0] = -light_dir[0] * 100.0f;
    light_pos[1] = -light_dir[1] * 100.0f;
    light_pos[2] = -light_dir[2] * 100.0f;
}

// ---------------------------------------------------------------
// Stencil shadow rendering
// ---------------------------------------------------------------
// Lighting with aggressive form shadows
// ---------------------------------------------------------------
void setup_lighting(void)
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Dramatic lighting: very low ambient, high diffuse contrast for form shadows
    float ambient[] = {0.15f, 0.15f, 0.2f, 1.0f};  // Very low ambient for deep shadows
    float diffuse[] = {1.0f, 0.95f, 0.85f, 1.0f};  // Strong warm sunlight
    float specular[] = {0.8f, 0.8f, 0.8f, 1.0f};   // Strong specular highlights
    float light_pos[] = {20.0f, 40.0f, 20.0f, 0.0f}; // Directional light

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    
    // Set material properties for dramatic shading
    float mat_ambient[] = {0.3f, 0.3f, 0.3f, 1.0f};
    float mat_diffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
    float mat_specular[] = {0.5f, 0.5f, 0.5f, 1.0f};
    float mat_shininess[] = {32.0f};
    
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
}

// ---------------------------------------------------------------
// main
// ---------------------------------------------------------------
int main(int argc, char *argv[])
{
    srand((unsigned int)time(NULL));

    // Init GLUT (needed only for glutBitmapCharacter HUD)
    glutInit(&argc, argv);

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    GLFWwindow *win = glfwCreateWindow(1280, 720,
                                       "Farm Windmill Sim", NULL, NULL);
    if (!win)
    {
        fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    glfwSetCursorPosCallback(win, mouse_callback);
    glfwSetScrollCallback(win, scroll_callback);
    glfwSetMouseButtonCallback(win, mouse_button_callback);
    glfwSetKeyCallback(win, key_callback);

    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK)
    {
        fprintf(stderr, "glewInit failed: %s\n", glewGetErrorString(glewErr));
        glfwTerminate();
        return 1;
    }
    glGetError();

    printf("OpenGL  : %s\n", glGetString(GL_VERSION));
    printf("GLSL    : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    setup_lighting();

    // Initialize sun position (fixed 5 o'clock)
    init_sun_position();

    // Load GLTF
    cgltf_options opt = {0};
    cgltf_result res = cgltf_parse_file(&opt, "assets/scene.gltf", &model_data);
    if (res == cgltf_result_success)
    {
        cgltf_load_buffers(&opt, model_data, "assets/scene.gltf");
        gl_textures = malloc(sizeof(GLuint) * model_data->images_count);
        for (cgltf_size i = 0; i < model_data->images_count; i++)
        {
            char path[512];
            snprintf(path, sizeof(path), "assets/%s", model_data->images[i].uri);
            gl_textures[i] = load_texture(path, 0);
        }
        printf("GLTF loaded : %d meshes, %d images\n",
               (int)model_data->meshes_count,
               (int)model_data->images_count);
    }
    else
    {
        fprintf(stderr, "Failed to load assets/scene.gltf (error %d)\n", res);
    }

    lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(win))
    {

        double t = glfwGetTime();
        lastTime = t;

        // Physics
        update_physics();

        glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int fbW, fbH;
        glfwGetFramebufferSize(win, &fbW, &fbH);
        if (fbH == 0)
            fbH = 1;
        glViewport(0, 0, fbW, fbH);

        // Projection
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (double)fbW / (double)fbH, 0.1, 500.0);

        // Camera
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        float pitchRad = glm_rad(camPitch);
        float yawRad = glm_rad(camYaw);
        float eyeX = sceneCX + camRadius * cosf(pitchRad) * sinf(yawRad);
        float eyeY = sceneCY + camRadius * sinf(pitchRad);
        float eyeZ = sceneCZ + camRadius * cosf(pitchRad) * cosf(yawRad);

        gluLookAt(eyeX, eyeY, eyeZ,
                  sceneCX, sceneCY, sceneCZ,
                  0.0, 1.0, 0.0);

        // Update light position based on fixed sun position (directional light)
        float light_pos_arr[] = {light_pos[0], light_pos[1], light_pos[2], 0.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos_arr);

        // Render scene
        if (model_data)
            for (cgltf_size i = 0; i < model_data->scenes[0].nodes_count; i++)
                render_node(model_data->scenes[0].nodes[i]);


        // HUD overlay
        render_hud(fbW, fbH);

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    if (gl_textures)
        free(gl_textures);
    if (model_data)
        cgltf_free(model_data);
    glfwTerminate();
    return 0;
}
