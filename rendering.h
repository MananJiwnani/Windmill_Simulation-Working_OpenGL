#ifndef RENDERING_H
#define RENDERING_H

/* Forward declarations */
typedef struct cgltf_primitive cgltf_primitive;
typedef struct cgltf_node cgltf_node;

/* Setup OpenGL lighting */
void setup_lighting(void);

/* Initialize sun position (fixed 5 o'clock) */
void init_sun_position(void);

/* Draw a single GLTF primitive */
void draw_primitive(cgltf_primitive *prim);

/* Recursively render GLTF nodes */
void render_node(cgltf_node *node);

#endif // RENDERING_H
