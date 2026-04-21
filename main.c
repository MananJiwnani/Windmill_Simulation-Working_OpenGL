#define _GNU_SOURCE
#include <GL/glew.h>
#include <GL/glut.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <cglm/cglm.h>

/* Single compilation unit for header-only libs */
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* Module Headers */
#include "state.h"
#include "physics.h"
#include "camera.h"
#include "input.h"
#include "rendering.h"
#include "effects.h"
#include "hud.h"
#include "assets.h"

/* ---------------------------------------------------------------
   Global State Definitions (initialized here)
   --------------------------------------------------------------- */

/* Physics & wind state */
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

/* Camera */
float camRadius = 35.0f;
float camPitch = 20.0f;
float camYaw = 45.0f;

const float sceneCX = -1.0f;
const float sceneCY = 3.0f;
const float sceneCZ = 0.0f;

double lastMouseX = 0.0, lastMouseY = 0.0;
int isRotating = 0;

/* GL / GLTF resources */
GLuint *gl_textures = NULL;
cgltf_data *model_data = NULL;

/* Shadow volumes (stencil shadows) */
int use_stencil_shadows = 1;

/* Night mode toggle */
int night_mode = 0;

/* Power threshold for house lights */
float current_power = 0.0;

/* Stars data */
float stars_x[NUM_STARS];
float stars_y[NUM_STARS];
float stars_z[NUM_STARS];
float stars_brightness[NUM_STARS];
int stars_initialized = 0;

/* Light properties - fixed 5 o'clock sun position */
double sun_azimuth = 165.0;   /* 5 o'clock position (165° from north) */
double sun_elevation = 40.0;  /* Afternoon elevation angle */

/* Current light direction */
float light_dir[3];  /* Normalized direction TO the light */
float light_pos[3];  /* Position far away */

/* ---------------------------------------------------------------
   GLFW error callback
   --------------------------------------------------------------- */
static void glfw_error_callback(int code, const char *desc)
{
    fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

/* ---------------------------------------------------------------
   Main Program
   --------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    srand((unsigned int)time(NULL));

    /* Init GLUT (needed only for glutBitmapCharacter HUD) */
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

    /* Register callbacks from camera and input modules */
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

    /* Initialize sun position (fixed 5 o'clock) */
    init_sun_position();

    /* Load GLTF model and textures from assets module */
    load_gltf_model("assets/scene.gltf", "assets");

    lastTime = glfwGetTime();

    /* ---------------------------------------------------------------
       Main Render Loop
       --------------------------------------------------------------- */
    while (!glfwWindowShouldClose(win))
    {
        double t = glfwGetTime();
        lastTime = t;

        /* Physics simulation */
        update_physics();

        /* Set background color based on night mode */
        if (night_mode)
        {
            glClearColor(0.05f, 0.08f, 0.15f, 1.0f); /* Dark blue night sky */
        }
        else
        {
            glClearColor(0.53f, 0.81f, 0.98f, 1.0f); /* Light blue day sky */
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        int fbW, fbH;
        glfwGetFramebufferSize(win, &fbW, &fbH);
        if (fbH == 0)
            fbH = 1;
        glViewport(0, 0, fbW, fbH);

        /* Projection */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (double)fbW / (double)fbH, 0.1, 500.0);

        /* Camera */
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

        /* Update lighting based on night mode */
        if (night_mode)
        {
            float ambient[] = {0.05f, 0.08f, 0.12f, 1.0f};  /* Minimal ambient */
            float diffuse[] = {0.3f, 0.3f, 0.4f, 1.0f};     /* Very low diffuse */
            glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
        }
        else
        {
            float ambient[] = {0.15f, 0.15f, 0.2f, 1.0f};   /* Low ambient */
            float diffuse[] = {1.0f, 0.95f, 0.85f, 1.0f};   /* Strong warm diffuse */
            glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
        }

        /* Update light position based on fixed sun position (directional light) */
        float light_pos_arr[] = {light_pos[0], light_pos[1], light_pos[2], 0.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos_arr);

        /* Update current power output */
        current_power = 1000.0 * fabs(torqueFact * wing_speed * wing_speed);

        /* Render 3D scene */
        if (model_data)
            for (cgltf_size i = 0; i < model_data->scenes[0].nodes_count; i++)
                render_node(model_data->scenes[0].nodes[i]);

        /* Render visual effects */
        render_stars();
        render_crescent_moon();
        render_house_lights();

        /* Render 2D HUD overlays */
        render_hud(fbW, fbH);
        render_wind_direction_arrow(fbW, fbH);

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    /* Cleanup */
    free_gltf_resources();
    glfwTerminate();
    return 0;
}
