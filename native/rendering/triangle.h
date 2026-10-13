#pragma once

#include <GLES3/gl3.h>
#include <string>

// All methods run on the thread with the current GL context.
class Triangle {
public:
    bool create(std::string& error);
    void draw(int width, int height, int colour);
    void destroy();

private:
    GLuint program_ = 0;
    GLuint vertices_ = 0;
    GLuint array_ = 0;
    GLint scale_ = -1;
    GLint tint_ = -1;
};
