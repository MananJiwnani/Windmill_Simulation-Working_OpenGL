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
double wing_z            = 0.0;
double wind_y            = 0.0;
double wing_speed        = 0.0;
double wind_acc_factor   = 0.8;
double turbine_factor    = 0.005;
double progstep          = 0.0;
double progstep_acc      = 0.0005;
double r_step            = 5.0;
double torqueFact        = 0.00002;

double wind_speed_target = 0.0;
double wind_angle_target = 0.0;
int    isRandom          = 0;

double lastTime = 0.0;

// Camera
float camRadius = 35.0f;
float camPitch  = 20.0f;
float camYaw    = 45.0f;

const float sceneCX = -1.0f;
const float sceneCY =  3.0f;
const float sceneCZ =  0.0f;

double lastMouseX = 0.0, lastMouseY = 0.0;
int    isRotating = 0;

// GL / GLTF resources
GLuint*     gl_textures = NULL;
cgltf_data* model_data  = NULL;

// Shadow volumes (stencil shadows)
int use_stencil_shadows = 1;

// Light properties - fixed 5 o'clock sun position
// 5 o'clock = 17:00, which is 11 hours after 6 AM
// Hour angle = (17 - 6) * 15 = 165°
double sun_azimuth = 165.0;  // 5 o'clock position (165° from north)
double sun_elevation = 40.0; // Afternoon elevation angle

// Current light direction (will be updated once at startup)
float light_dir[3];          // Normalized direction TO the light
float light_pos[3];          // Position far away


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
    (void)scancode; (void)mods;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        // Wind speed
        if (key == GLFW_KEY_UP) {
            if (progstep < 0.3) progstep += progstep_acc;
        }
        if (key == GLFW_KEY_DOWN) {
            if (progstep > -0.3) progstep -= progstep_acc;
        }
        // Wind direction
        if (key == GLFW_KEY_A) wind_y -= r_step;
        if (key == GLFW_KEY_D) wind_y += r_step;
        // Camera orbit
        if (key == GLFW_KEY_LEFT)  camYaw -= (float)r_step;
        if (key == GLFW_KEY_RIGHT) camYaw += (float)r_step;
        // Camera pitch
        if (key == GLFW_KEY_W) {
            camPitch += 2.0f;
            if (camPitch > 89.0f) camPitch = 89.0f;
        }
        if (key == GLFW_KEY_S) {
            camPitch -= 2.0f;
            if (camPitch < -5.0f) camPitch = -5.0f;
        }
        // Toggle random wind
        if (key == GLFW_KEY_R) {
            isRandom = 1 - isRandom;
            wind_speed_target = (double)(rand() % 50);
            wind_angle_target = (double)(rand() % 360);
        }
        // Toggle stencil shadows
        if (key == GLFW_KEY_T) {
            use_stencil_shadows = 1 - use_stencil_shadows;
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
    if (isRandom) {
        if (rand() % 1000 == 0) {
            wind_speed_target = (double)(rand() % 50);
            wind_angle_target = (double)(rand() % 360);
        }
        progstep += (0.008 * (wind_speed_target - progstep * 1000.0)) / 1000.0;
        wind_y   += 0.002 * (wind_angle_target - wind_y);
    }

    double acc = progstep * cos(wind_y / 180.0 * M_PI) * wind_acc_factor
                 - wing_speed * turbine_factor;
    wing_speed += acc;
    wing_z     += wing_speed;

    if (wing_z >= 360.0) wing_z -= 360.0;
    if (wing_z <    0.0) wing_z += 360.0;
}


// ---------------------------------------------------------------
// HUD rendering (ortho overlay)
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
    float line_h = 28.0f;
    float x      = 20.0f;
    float y      = (float)fbH - 35.0f;

    // Draw semi-transparent background panel
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.65f);
    glBegin(GL_QUADS);
    glVertex2f(10.0f, (float)fbH - 330.0f);
    glVertex2f(500.0f, (float)fbH - 330.0f);
    glVertex2f(500.0f, (float)fbH - 20.0f);
    glVertex2f(10.0f, (float)fbH - 20.0f);
    glEnd();
    glDisable(GL_BLEND);

    // Title
    glColor3f(1.0f, 0.95f, 0.3f);
    glRasterPos2f(x, y);
    const char* title = "WINDMILL FARM SIM";
    for (const char* c = title; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    y -= line_h;

    // Divider
    glColor3f(0.7f, 0.7f, 0.7f);
    glRasterPos2f(x, y);
    const char* div = "-----------------------------";
    for (const char* c = div; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    y -= line_h;

    // Wind speed
    glColor3f(0.3f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Wind Speed : %.3f km/hr",
             1000.0 * fabs(progstep));
    for (char* c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    y -= line_h;

    // Wind direction
    glColor3f(0.5f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Wind Dir   : %.1f deg", wind_y);
    for (char* c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    y -= line_h;

    // Wing speed
    glColor3f(1.0f, 0.5f, 0.5f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Blade Speed: %.4f deg/frame", wing_speed);
    for (char* c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    y -= line_h;

    // Power
    glColor3f(1.0f, 1.0f, 0.8f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Power      : %.4f MW",
             1000.0 * fabs(torqueFact * wing_speed * wing_speed));
    for (char* c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    y -= line_h;

    // Random mode
    glColor3f(isRandom ? 0.5f : 0.8f,
              isRandom ? 1.0f : 0.8f,
              0.3f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Random Wind: %s", isRandom ? "ON" : "OFF");
    for (char* c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    y -= (line_h * 1.5f);

    // Controls hint
    glColor3f(0.8f, 0.8f, 0.8f);
    glRasterPos2f(x, y);
    const char* hint1 = "UP/DOWN: wind speed   A/D: wind dir";
    for (const char* c = hint1; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    y -= 18.0f;

    glRasterPos2f(x, y);
    const char* hint2 = "LEFT/RIGHT: orbit   W/S: pitch   R: random";
    for (const char* c = hint2; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    y -= 18.0f;

    glRasterPos2f(x, y);
    const char* hint3 = "Mouse drag: orbit   Scroll: zoom   ESC: quit";
    for (const char* c = hint3; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    y -= 18.0f;

    glRasterPos2f(x, y);
    const char* hint4 = "+/-: speed up/slow down sun   T: toggle shadows";
    for (const char* c = hint4; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
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
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // Check for texture and color from material
    if (prim->material && prim->material->has_pbr_metallic_roughness) {
        // Try to apply baseColorTexture
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

        // Apply baseColorFactor if present (even if texture is used, for modulation)
        float* bcf = prim->material->pbr_metallic_roughness.base_color_factor;
        if (bcf) {
            for (int j = 0; j < 4; j++)
                color[j] = bcf[j];
        }
    }

    glColor4fv(color);

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
// ---------------------------------------------------------------
void render_node(cgltf_node* node)
{
    glPushMatrix();

    mat4 local;
    cgltf_node_transform_local(node, (float*)local);
    glMultMatrixf((float*)local);

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
void render_shadow_volume(void)
{
    // Render shadow volumes using stencil buffer with more dramatic effect
    
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0);
    glStencilOp(GL_KEEP, GL_INCR_WRAP, GL_INCR_WRAP);
    
    // Render scene geometry to stencil buffer
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    
    if (model_data)
        for (cgltf_size i = 0; i < model_data->scenes[0].nodes_count; i++)
            render_node(model_data->scenes[0].nodes[i]);
    
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    
    // Now render shadow pass - darken areas where stencil > 0
    glStencilFunc(GL_GREATER, 0, ~0);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    
    glDisable(GL_LIGHTING);
    glColor4f(0.0f, 0.0f, 0.0f, 0.75f);  // Increased from 0.5 to 0.75 for more dramatic shadows
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw a full-screen quad to apply shadow tint
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glBegin(GL_QUADS);
    glVertex3f(0, 0, 0);
    glVertex3f(1, 0, 0);
    glVertex3f(1, 1, 0);
    glVertex3f(0, 1, 0);
    glEnd();
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    
    glDisable(GL_STENCIL_TEST);
    glClear(GL_STENCIL_BUFFER_BIT);
}


// ---------------------------------------------------------------
// Lighting
// ---------------------------------------------------------------
void setup_lighting(void)
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Light properties - position will be updated each frame based on sun_hour
    float ambient[]   = {  0.3f,  0.3f,  0.4f, 1.0f };  // Slightly cooler ambient
    float diffuse[]   = {  1.0f,  0.95f,  0.85f, 1.0f };  // Warm sunlight
    float specular[]  = {  0.5f,  0.5f,  0.5f, 1.0f };

    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    
    // Add a fill light (secondary light) to soften shadows
    glEnable(GL_LIGHT1);
    float fill_ambient[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float fill_diffuse[] = { 0.3f, 0.4f, 0.5f, 1.0f };  // Cool blue fill light
    
    glLightfv(GL_LIGHT1, GL_AMBIENT,  fill_ambient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  fill_diffuse);
}


// ---------------------------------------------------------------
// main
// ---------------------------------------------------------------
int main(int argc, char* argv[])
{
    srand((unsigned int)time(NULL));

    // Init GLUT (needed only for glutBitmapCharacter HUD)
    glutInit(&argc, argv);

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

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

    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
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

    lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(win)) {

        double t = glfwGetTime();
        lastTime = t;

        // Physics
        update_physics();

        glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        int fbW, fbH;
        glfwGetFramebufferSize(win, &fbW, &fbH);
        if (fbH == 0) fbH = 1;
        glViewport(0, 0, fbW, fbH);

        // Projection
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (double)fbW / (double)fbH, 0.1, 500.0);

        // Camera
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

        // Update light position based on current sun position (directional light)
        float light_pos_arr[] = { light_pos[0], light_pos[1], light_pos[2], 0.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos_arr);
        
        // Fill light positioned opposite and to the side for soft shadows
        float fill_pos[] = { light_pos[0] + 30.0f, 30.0f, light_pos[2] + 30.0f, 0.0f };
        glLightfv(GL_LIGHT1, GL_POSITION, fill_pos);

        // Render scene
        if (model_data)
            for (cgltf_size i = 0; i < model_data->scenes[0].nodes_count; i++)
                render_node(model_data->scenes[0].nodes[i]);

        // Apply stencil shadows
        if (use_stencil_shadows)
            render_shadow_volume();

        // HUD overlay
        render_hud(fbW, fbH);

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    if (gl_textures) free(gl_textures);
    if (model_data)  cgltf_free(model_data);
    glfwTerminate();
    return 0;
}
