#include "triangle.h"

#include <algorithm>

namespace {
constexpr const char* vertex_source = R"(#version 300 es
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 colour;
uniform vec2 scale;
out vec3 vertex_colour;
void main() {
    gl_Position = vec4(position * scale, 0.0, 1.0);
    vertex_colour = colour;
}
)";
constexpr const char* fragment_source = R"(#version 300 es
precision mediump float;
in vec3 vertex_colour;
uniform vec3 tint;
out vec4 pixel;
void main() { pixel = vec4(vertex_colour * tint, 1.0); }
)";

GLuint compile(GLenum type, const char* source, std::string& error) {
    const GLuint shader = glCreateShader(type);
    if (!shader) {
        error = "Cannot allocate shader";
        return 0;
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint okay = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &okay);
    if (!okay) {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        error = std::string("Shader compilation failed: ") + log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
} // namespace

bool Triangle::create(std::string& error) {
    const GLuint vertex = compile(GL_VERTEX_SHADER, vertex_source, error);
    if (!vertex) return false;
    const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragment_source, error);
    if (!fragment) {
        glDeleteShader(vertex);
        return false;
    }
    program_ = glCreateProgram();
    if (program_) {
        glAttachShader(program_, vertex);
        glAttachShader(program_, fragment);
        glLinkProgram(program_);
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    if (!program_) {
        error = "Cannot allocate shader program";
        return false;
    }
    GLint okay = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &okay);
    if (!okay) {
        char log[2048]{};
        glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        error = std::string("Shader linking failed: ") + log;
        destroy();
        return false;
    }
    scale_ = glGetUniformLocation(program_, "scale");
    tint_ = glGetUniformLocation(program_, "tint");
    constexpr float data[] = {
        -0.7f, -0.6f, 1.0f, 0.2f, 0.2f,
         0.7f, -0.6f, 0.2f, 1.0f, 0.2f,
         0.0f,  0.7f, 0.2f, 0.4f, 1.0f,
    };
    glGenVertexArrays(1, &array_);
    glBindVertexArray(array_);
    glGenBuffers(1, &vertices_);
    glBindBuffer(GL_ARRAY_BUFFER, vertices_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          reinterpret_cast<const void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    if (glGetError() != GL_NO_ERROR || scale_ < 0 || tint_ < 0 || !array_ || !vertices_) {
        error = "Cannot initialise triangle buffers or uniforms";
        destroy();
        return false;
    }
    return true;
}

void Triangle::draw(int width, int height, int colour) {
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    glClearColor(0.035f, 0.05f, 0.075f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program_);
    glUniform2f(scale_, std::min(1.0f, float(height) / float(width)),
                       std::min(1.0f, float(width) / float(height)));
    constexpr float tints[][3] = {{1, 1, 1}, {1, 0.35f, 0.35f}, {0.35f, 1, 0.35f}};
    const auto& tint = tints[colour % 3];
    glUniform3fv(tint_, 1, tint);
    glBindVertexArray(array_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void Triangle::destroy() {
    if (vertices_) glDeleteBuffers(1, &vertices_);
    if (array_) glDeleteVertexArrays(1, &array_);
    if (program_) glDeleteProgram(program_);
    vertices_ = array_ = program_ = 0;
}
