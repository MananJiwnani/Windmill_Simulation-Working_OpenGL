#define _GNU_SOURCE
#include <stdlib.h>
#include <math.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include "state.h"
#include "effects.h"

void init_stars(void)
{
    if (stars_initialized)
        return;

    srand(12345); /* Fixed seed for consistent star positions */

    for (int i = 0; i < NUM_STARS; i++)
    {
        /* Generate random position on a far-away sphere */
        float theta = (float)(rand() % 360) * M_PI / 180.0f;
        float phi = (float)(rand() % 180) * M_PI / 180.0f;
        float radius = 300.0f; /* Far away sphere */

        stars_x[i] = radius * sinf(phi) * cosf(theta);
        stars_y[i] = radius * cosf(phi);
        stars_z[i] = radius * sinf(phi) * sinf(theta);

        /* Random brightness (0.3 to 1.0) */
        stars_brightness[i] = 0.3f + (float)(rand() % 70) / 100.0f;
    }

    stars_initialized = 1;
}

void render_stars(void)
{
    if (!night_mode)
        return;

    init_stars();

    glDisable(GL_LIGHTING);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glPointSize(2.0f); /* Moderate star size */

    glBegin(GL_POINTS);
    for (int i = 0; i < NUM_STARS; i++)
    {
        /* Twinkle effect using sine wave based on star index and time */
        float twinkle = 0.5f + 0.5f * sinf(glfwGetTime() * 2.0f + (float)i);
        float brightness = stars_brightness[i] * twinkle;

        glColor3f(brightness, brightness, brightness * 0.95f); /* Slight yellow tint */
        glVertex3f(stars_x[i], stars_y[i], stars_z[i]);
    }
    glEnd();

    glDisable(GL_POINT_SMOOTH);
    glPointSize(1.0f);
    glEnable(GL_LIGHTING);
}

void render_crescent_moon(void)
{
    if (!night_mode)
        return;

    /* Position moon in front of the scene (towards camera) */
    float moon_x = sceneCX - 15.0f;
    float moon_y = sceneCY + 20.0f;
    float moon_z = sceneCZ + 25.0f; /* Positive z = in front of scene */

    glPushMatrix();
    glTranslatef(moon_x, moon_y, moon_z);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Draw bright crescent part */
    glColor4f(1.0f, 0.95f, 0.75f, 0.95f);
    GLUquadric *quad = gluNewQuadric();
    gluSphere(quad, 2.0f, 64, 64);

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void render_house_lights(void)
{
    /* Only show lights at night and when power output is sufficient */
    if (!night_mode || current_power < POWER_THRESHOLD)
        return;

    /* Calculate brightness based on power output (normalized) */
    float brightness = fminf((current_power - POWER_THRESHOLD) / 0.5f, 1.0f);
    brightness = fmaxf(brightness, 0.6f); /* Min brightness */

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Warm yellow light color for windows */
    glColor4f(1.0f, 0.95f, 0.7f, brightness);

    /* Window data: position and rotation (in degrees around Y axis) */
    typedef struct
    {
        float x, y, z;
        float rotation; /* 0, 90, 180, 270 degrees */
    } WindowLight;

    WindowLight windows[] = {
        /* Front/back windows (0 degree rotation - looking along Z axis) */
        {sceneCX + 2.65f, sceneCY - 1.0f, sceneCZ + 3.54f, 0.0f},
        {sceneCX - 0.3f, sceneCY - 1.0f, sceneCZ - 3.58f, 0.0f},
        {sceneCX + 2.55f, sceneCY - 1.0f, sceneCZ - 3.58f, 180.0f},

        /* Left side windows (90 degree rotation - looking along X axis) */
        {sceneCX + 0.98f, sceneCY - 1.0f, sceneCZ + 2.2f, 90.0f},
        {sceneCX - 2.58f, sceneCY - 1.0f, sceneCZ - 1.55f, 90.0f},

        /* Right side windows (270 degree rotation - looking along X axis) */
        {sceneCX + 4.58f, sceneCY - 1.0f, sceneCZ + 1.2f, 270.0f},
        {sceneCX + 4.58f, sceneCY - 1.0f, sceneCZ - 1.8f, 270.0f},
    };

    int num_windows = sizeof(windows) / sizeof(windows[0]);

    float xsize = 0.4f;
    float ysize = 0.6f;

    /* Draw each window with its own transformation */
    for (int i = 0; i < num_windows; i++)
    {
        glPushMatrix();
        glTranslatef(windows[i].x, windows[i].y, windows[i].z);
        glRotatef(windows[i].rotation, 0.0f, 1.0f, 0.0f);

        glBegin(GL_QUADS);
        /* Draw window quad centered at origin */
        glVertex3f(-xsize, -ysize, 0.0f);
        glVertex3f(xsize, -ysize, 0.0f);
        glVertex3f(xsize, ysize, 0.0f);
        glVertex3f(-xsize, ysize, 0.0f);
        glEnd();

        glPopMatrix();
    }

    /* Add glow effect around windows */
    glColor4f(1.0f, 0.95f, 0.7f, brightness * 0.3f);
    glPointSize(8.0f);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

    glBegin(GL_POINTS);
    for (int i = 0; i < num_windows; i++)
    {
        glVertex3f(windows[i].x, windows[i].y, windows[i].z);
    }
    glEnd();

    glDisable(GL_POINT_SMOOTH);
    glPointSize(1.0f);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}
