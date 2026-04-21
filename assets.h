#ifndef ASSETS_H
#define ASSETS_H

#include <GL/glew.h>

/* Load texture from file */
GLuint load_texture(const char *path, int repeat);

/* Load GLTF model (returns success status) */
int load_gltf_model(const char *model_path, const char *asset_dir);

/* Free loaded resources */
void free_gltf_resources(void);

#endif // ASSETS_H
