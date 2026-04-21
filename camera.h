#ifndef CAMERA_H
#define CAMERA_H

#include <GLFW/glfw3.h>

/* Camera movement callbacks */
void mouse_button_callback(GLFWwindow *w, int b, int a, int mods);
void mouse_callback(GLFWwindow *w, double x, double y);
void scroll_callback(GLFWwindow *w, double dx, double dy);

#endif // CAMERA_H
