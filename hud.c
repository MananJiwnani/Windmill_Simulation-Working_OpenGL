#define _GNU_SOURCE
#include <GL/glew.h>
#include <GL/glut.h>
#include <stdio.h>
#include <math.h>
#include "state.h"
#include "hud.h"

void render_wind_direction_arrow(int fbW, int fbH)
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

    /* Wind compass position (below dashboard in leftmost corner) */
    float compass_x = 400.0f;              /* Relative to left edge */
    float compass_y = (float)fbH - 105.0f; /* Below dashboard */
    float compass_radius = 30.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Draw compass circle background */
    glColor4f(0.1f, 0.2f, 0.3f, 0.7f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(compass_x, compass_y);
    for (int i = 0; i <= 32; i++)
    {
        float angle = (float)i / 32.0f * 2.0f * M_PI;
        glVertex2f(compass_x + cosf(angle) * compass_radius,
                   compass_y + sinf(angle) * compass_radius);
    }
    glEnd();

    /* Draw compass circle outline */
    glColor4f(0.0f, 0.7f, 1.0f, 0.6f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 32; i++)
    {
        float angle = (float)i / 32.0f * 2.0f * M_PI;
        glVertex2f(compass_x + cosf(angle) * compass_radius,
                   compass_y + sinf(angle) * compass_radius);
    }
    glEnd();

    /* Draw wind direction arrow pointing toward wind source */
    float wind_angle_rad = (90.0f - (float)wind_y) * M_PI / 180.0f;
    float arrow_length = compass_radius * 0.65f;
    float arrow_end_x = compass_x + cosf(wind_angle_rad) * arrow_length;
    float arrow_end_y = compass_y + sinf(wind_angle_rad) * arrow_length;

    /* Arrow shaft (bright orange/red) */
    glColor3f(1.0f, 0.4f, 0.2f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glVertex2f(compass_x, compass_y);
    glVertex2f(arrow_end_x, arrow_end_y);
    glEnd();
    glLineWidth(1.0f);

    /* Arrow head (triangular) */
    float arrow_size = 8.0f;
    float head_angle1 = wind_angle_rad - 0.4f;
    float head_angle2 = wind_angle_rad + 0.4f;

    glColor3f(1.0f, 0.6f, 0.3f);
    glBegin(GL_TRIANGLES);
    glVertex2f(arrow_end_x, arrow_end_y);
    glVertex2f(arrow_end_x - cosf(head_angle1) * arrow_size,
               arrow_end_y - sinf(head_angle1) * arrow_size);
    glVertex2f(arrow_end_x - cosf(head_angle2) * arrow_size,
               arrow_end_y - sinf(head_angle2) * arrow_size);
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

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
    float panel_y = (float)fbH - 350.0f;
    float panel_w = 480.0f;
    float panel_h = 330.0f;

    /* Modern dark background with gradient effect */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Main panel background - modern dark */
    glColor4f(0.08f, 0.1f, 0.15f, 0.85f);
    glBegin(GL_QUADS);
    glVertex2f(panel_x, panel_y);
    glVertex2f(panel_x + panel_w, panel_y);
    glVertex2f(panel_x + panel_w, panel_y + panel_h);
    glVertex2f(panel_x, panel_y + panel_h);
    glEnd();

    /* border outline - bright cyan with some transparency */
    glColor4f(0.0f, 0.7f, 1.0f, 0.6f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(panel_x + 1.5f, panel_y + 1.5f);
    glVertex2f(panel_x + panel_w - 1.5f, panel_y + 1.5f);
    glVertex2f(panel_x + panel_w - 1.5f, panel_y + panel_h - 1.5f);
    glVertex2f(panel_x + 1.5f, panel_y + panel_h - 1.5f);
    glEnd();

    glDisable(GL_BLEND);

    float x = panel_x + 20.0f;
    float y = panel_y + panel_h - 30.0f;
    float line_spacing = 26.0f;

    /* Title - Modern styling */
    glColor3f(0.0f, 0.8f, 1.0f);
    glRasterPos2f(x, y);
    const char *title = "WINDMILL SIMULATION";
    for (const char *c = title; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    y -= line_spacing - 10.0f;

    /* Section divider */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.7f, 1.0f, 0.3f);
    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x + 400.0f, y);
    glEnd();
    glDisable(GL_BLEND);

    y -= 22.0f;

    /* Wind speed with progress bar */
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Speed:");
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    glColor3f(0.2f, 1.0f, 0.8f);
    glRasterPos2f(x + 120.0f, y);
    snprintf(hud, sizeof(hud), "%.2f km/hr", 1000.0 * fabs(progstep));
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    y -= line_spacing;

    /* Wind direction */
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Direction:");
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    glColor3f(0.2f, 1.0f, 0.8f);
    glRasterPos2f(x + 150.0f, y);
    snprintf(hud, sizeof(hud), "%.0f°", wind_y);
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    y -= line_spacing + 6.0f;

    /* TURBINE METRICS */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 0.5f, 0.0f, 0.2f);
    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x + 400.0f, y);
    glEnd();
    glDisable(GL_BLEND);
    y -= 22.0f;

    /* Blade speed */
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Blade Speed:");
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    glColor3f(1.0f, 0.6f, 0.2f);
    glRasterPos2f(x + 150.0f, y);
    snprintf(hud, sizeof(hud), "%.1f°/fr", wing_speed);
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    y -= line_spacing;

    /* Power output */
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Output:");
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    double power = 1000.0 * fabs(torqueFact * wing_speed * wing_speed);
    glColor3f(0.2f, 1.0f, 0.3f);
    glRasterPos2f(x + 120.0f, y);
    snprintf(hud, sizeof(hud), "%.4f MW", power);
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    y -= line_spacing;

    /* STATUS SECTION */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.8f, 0.5f, 0.2f, 0.2f);
    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x + 400.0f, y);
    glEnd();
    glDisable(GL_BLEND);
    y -= 22.0f;

    /* Random mode indicator */
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Status:");
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    if (isRandom)
    {
        glColor3f(0.2f, 1.0f, 0.5f);
        glRasterPos2f(x + 120.0f, y);
        const char *status = "● RANDOM WIND";
        for (const char *c = status; *c; c++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    else
    {
        glColor3f(1.0f, 0.9f, 0.2f);
        glRasterPos2f(x + 120.0f, y);
        const char *status = "● MANUAL CONTROL";
        for (const char *c = status; *c; c++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    y -= line_spacing;

    /* Night mode indicator */
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    snprintf(hud, sizeof(hud), "Light:");
    for (char *c = hud; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    if (night_mode)
    {
        glColor3f(0.5f, 0.7f, 1.0f);
        glRasterPos2f(x + 120.0f, y);
        const char *light_status = "● NIGHT MODE";
        for (const char *c = light_status; *c; c++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    else
    {
        glColor3f(1.0f, 0.8f, 0.2f);
        glRasterPos2f(x + 120.0f, y);
        const char *light_status = "● DAY MODE";
        for (const char *c = light_status; *c; c++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    y -= line_spacing + 8.0f;

    /* FOOTER - Controls */
    glColor3f(0.5f, 0.6f, 0.7f);
    glRasterPos2f(x, y);
    const char *footer = "[ Mouse Drag: Orbit  |  R: Random  |  N: Night Mode  |  ESC: Quit ]";
    for (const char *c = footer; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
    y -= 18.0f;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
