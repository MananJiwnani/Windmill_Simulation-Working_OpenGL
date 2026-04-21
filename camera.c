#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "state.h"
#include "camera.h"

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
