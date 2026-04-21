#define _GNU_SOURCE
#include <stdlib.h>
#include <math.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "state.h"
#include "input.h"

void key_callback(GLFWwindow *w, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        /* Wind speed */
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
        /* Wind direction */
        if (key == GLFW_KEY_LEFT)
        {
            wind_y -= r_step;
            wind_y = fmod(wind_y, 360.0);
            if (wind_y < 0.0) wind_y += 360.0;
        }
        if (key == GLFW_KEY_RIGHT)
        {
            wind_y += r_step;
            wind_y = fmod(wind_y, 360.0);
            if (wind_y < 0.0) wind_y += 360.0;
        }
        /* Camera orbit */
        if (key == GLFW_KEY_A)
            camYaw -= (float)r_step;
        if (key == GLFW_KEY_D)
            camYaw += (float)r_step;
        /* Camera pitch */
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
        /* Toggle random wind */
        if (key == GLFW_KEY_R)
        {
            isRandom = 1 - isRandom;
            wind_speed_target = (double)(rand() % 50);
            wind_angle_target = (double)(rand() % 360);
        }
        /* Toggle stencil shadows */
        if (key == GLFW_KEY_T)
        {
            use_stencil_shadows = 1 - use_stencil_shadows;
        }
        /* Toggle night mode */
        if (key == GLFW_KEY_N)
        {
            night_mode = 1 - night_mode;
        }
        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(w, GLFW_TRUE);
        /* Camera view mode switching */
        if (key == GLFW_KEY_F)
            camera_view_mode = CAMERA_ORTHO_FRONT;
        if (key == GLFW_KEY_L)
            camera_view_mode = CAMERA_ORTHO_LEFT;
        if (key == GLFW_KEY_R)
            camera_view_mode = CAMERA_ORTHO_RIGHT;
        if (key == GLFW_KEY_T)
            camera_view_mode = CAMERA_ORTHO_TOP;
        if (key == GLFW_KEY_P)
            camera_view_mode = CAMERA_PERSPECTIVE;
    }
}
