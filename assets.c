#include <GL/glew.h>
#include <stdio.h>
#include <string.h>
#include "cgltf.h"
#include "stb_image.h"
#include "state.h"
#include "assets.h"

GLuint load_texture(const char *path, int repeat)
{
    GLuint tex;
    glGenTextures(1, &tex);

    int w, h, ch;
    unsigned char *data = stbi_load(path, &w, &h, &ch, 0);
    if (data)
    {
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
    }
    else
    {
        printf("FAILED texture : %s\n", path);
    }
    return tex;
}

int load_gltf_model(const char *model_path, const char *asset_dir)
{
    cgltf_options opt = {0};
    cgltf_result res = cgltf_parse_file(&opt, model_path, &model_data);
    if (res == cgltf_result_success)
    {
        cgltf_load_buffers(&opt, model_data, model_path);
        gl_textures = malloc(sizeof(GLuint) * model_data->images_count);
        for (cgltf_size i = 0; i < model_data->images_count; i++)
        {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", asset_dir, model_data->images[i].uri);
            gl_textures[i] = load_texture(path, 0);
        }
        printf("GLTF loaded : %d meshes, %d images\n",
               (int)model_data->meshes_count,
               (int)model_data->images_count);
        return 1;
    }
    else
    {
        fprintf(stderr, "Failed to load %s (error %d)\n", model_path, res);
        return 0;
    }
}

void free_gltf_resources(void)
{
    if (gl_textures)
        free(gl_textures);
    if (model_data)
        cgltf_free(model_data);
}
