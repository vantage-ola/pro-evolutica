#pragma once

#include <GLES3/gl3.h>
#include <string>
#include "../assets/png.h"

// All methods run on the thread with the current GL context.
class Cube {
public:
    bool create(std::string& error);
    bool set_texture(const assets::Image& image, std::string& error);
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
};
