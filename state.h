#ifndef STATE_H
#define STATE_H

#include <GL/glew.h>
#define GLFW_INCLUDE_NONE

/* Forward declaration - don't include cgltf.h here to avoid multiple includes */
typedef struct cgltf_data cgltf_data;

/* ========================================
   Physics & Wind State
   ======================================== */
extern double wing_z;
extern double wind_y;
extern double wing_speed;
extern double wind_acc_factor;
extern double turbine_factor;
extern double progstep;
extern double progstep_acc;
extern double r_step;
extern double torqueFact;
extern double wind_speed_target;
extern double wind_angle_target;
extern int isRandom;
extern double lastTime;

/* ========================================
   Camera Control
   ======================================== */
extern float camRadius;
extern float camPitch;
extern float camYaw;
extern const float sceneCX;
extern const float sceneCY;
extern const float sceneCZ;
extern double lastMouseX;
extern double lastMouseY;
extern int isRotating;

/* ========================================
   GL & GLTF Resources
   ======================================== */
extern GLuint *gl_textures;
extern cgltf_data *model_data;

/* ========================================
   Visual Features
   ======================================== */
extern int use_stencil_shadows;
extern int night_mode;
extern float current_power;

#define POWER_THRESHOLD 0.075
#define NUM_STARS 1000

extern float stars_x[NUM_STARS];
extern float stars_y[NUM_STARS];
extern float stars_z[NUM_STARS];
extern float stars_brightness[NUM_STARS];
extern int stars_initialized;

/* ========================================
   Lighting
   ======================================== */
extern double sun_azimuth;
extern double sun_elevation;
extern float light_dir[3];
extern float light_pos[3];

#endif // STATE_H
