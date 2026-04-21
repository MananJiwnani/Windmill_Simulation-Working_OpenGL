#define _GNU_SOURCE
#include <GL/glew.h>
#include <GL/glut.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cglm/cglm.h>
#include "cgltf.h"
#include "state.h"
#include "rendering.h"

void setup_lighting(void)
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    /* Dramatic lighting: very low ambient, high diffuse contrast for form shadows */
    float ambient[] = {0.15f, 0.15f, 0.2f, 1.0f};    /* Very low ambient for deep shadows */
    float diffuse[] = {1.0f, 0.95f, 0.85f, 1.0f};    /* Strong warm sunlight */
    float specular[] = {0.8f, 0.8f, 0.8f, 1.0f};     /* Strong specular highlights */
    float light_pos[] = {20.0f, 40.0f, 20.0f, 0.0f}; /* Directional light */

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

    /* Set material properties for dramatic shading */
    float mat_ambient[] = {0.3f, 0.3f, 0.3f, 1.0f};
    float mat_diffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
    float mat_specular[] = {0.5f, 0.5f, 0.5f, 1.0f};
    float mat_shininess[] = {32.0f};

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
}

void init_sun_position(void)
{
    /* Convert hour angle and elevation to radians */
    float azimuth = glm_rad((float)sun_azimuth);
    float elev_rad = glm_rad((float)sun_elevation);

    /* Calculate light direction (FROM sun TO scene) */
    light_dir[0] = sinf(azimuth) * cosf(elev_rad);
    light_dir[1] = sinf(elev_rad);
    light_dir[2] = cosf(azimuth) * cosf(elev_rad);

    /* Calculate light position (far away in opposite direction) */
    light_pos[0] = -light_dir[0] * 100.0f;
    light_pos[1] = -light_dir[1] * 100.0f;
    light_pos[2] = -light_dir[2] * 100.0f;
}

void draw_primitive(cgltf_primitive *prim)
{
    if (!prim->indices)
        return;

    cgltf_accessor *pos = NULL, *uv = NULL, *norm = NULL;
    for (cgltf_size i = 0; i < prim->attributes_count; i++)
    {
        switch (prim->attributes[i].type)
        {
        case cgltf_attribute_type_position:
            pos = prim->attributes[i].data;
            break;
        case cgltf_attribute_type_texcoord:
            uv = prim->attributes[i].data;
            break;
        case cgltf_attribute_type_normal:
            norm = prim->attributes[i].data;
            break;
        default:
            break;
        }
    }
    if (!pos)
        return;

    int has_tex = 0;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /* Check for texture and color from material */
    if (prim->material && prim->material->has_pbr_metallic_roughness)
    {
        /* Try to apply baseColorTexture */
        cgltf_texture *t =
            prim->material->pbr_metallic_roughness.base_color_texture.texture;
        if (t && t->image)
        {
            for (cgltf_size i = 0; i < model_data->images_count; i++)
            {
                if (&model_data->images[i] == t->image)
                {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, gl_textures[i]);
                    has_tex = 1;
                    break;
                }
            }
        }

        /* Apply baseColorFactor if present (even if texture is used, for modulation) */
        float *bcf = prim->material->pbr_metallic_roughness.base_color_factor;
        if (bcf)
        {
            for (int j = 0; j < 4; j++)
                color[j] = bcf[j];
        }
    }

    glColor4fv(color);

    glBegin(GL_TRIANGLES);
    for (cgltf_size i = 0; i < prim->indices->count; i++)
    {
        int idx = (int)cgltf_accessor_read_index(prim->indices, i);
        if (norm)
        {
            float n[3];
            cgltf_accessor_read_float(norm, idx, n, 3);
            glNormal3fv(n);
        }
        if (uv)
        {
            float u[2];
            cgltf_accessor_read_float(uv, idx, u, 2);
            glTexCoord2fv(u);
        }
        float p[3];
        cgltf_accessor_read_float(pos, idx, p, 3);
        glVertex3fv(p);
    }
    glEnd();

    if (has_tex)
        glDisable(GL_TEXTURE_2D);
}

void render_node(cgltf_node *node)
{
    glPushMatrix();

    mat4 local;
    cgltf_node_transform_local(node, (float *)local);
    glMultMatrixf((float *)local);

    if (node->name && strcasestr(node->name, "prop"))
        glRotatef((float)wing_z, 0.0f, 1.0f, 0.0f);

    if (node->mesh)
        for (cgltf_size i = 0; i < node->mesh->primitives_count; i++)
            draw_primitive(&node->mesh->primitives[i]);

    for (cgltf_size i = 0; i < node->children_count; i++)
        render_node(node->children[i]);

    glPopMatrix();
}
