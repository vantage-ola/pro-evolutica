#pragma once

#include <GLES3/gl3.h>
#include <string>
#include "../assets/png.h"
#include "../assets/mesh.h"

// All methods run on the thread with the current GL context.
class Cube {
public:
    bool create(std::string& error);
    bool set_texture(const assets::Image& image, std::string& error);
    bool set_mesh(const assets::Mesh& mesh, std::string& error);
    void draw(int width, int height, int colour, float angle);
    void destroy();

private:
    GLuint program_ = 0;
    GLuint vertices_ = 0;
    GLuint array_ = 0;
    GLuint indices_ = 0;
    GLuint texture_ = 0;
    GLint transform_ = -1;
    GLint tint_ = -1;
    GLint rotation_ = -1;
    GLint mesh_mode_ = -1;
    GLint centre_uniform_ = -1;
    GLint scale_uniform_ = -1;
    std::array<float, 3> centre_{};
    float scale_ = 1;
    bool mesh_ = false;
    GLsizei index_count_ = 36;
};
