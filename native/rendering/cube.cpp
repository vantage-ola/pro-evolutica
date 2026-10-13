#include "cube.h"
#include "cube_data.h"

#include <cstddef>

namespace {
constexpr const char* vertex_source = R"(#version 300 es
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in float light;
uniform mat4 transform;
out vec2 texture_uv;
out float face_light;
void main() {
    gl_Position = transform * vec4(position, 1.0);
    texture_uv = uv;
    face_light = light;
}
)";
constexpr const char* fragment_source = R"(#version 300 es
precision mediump float;
in vec2 texture_uv;
in float face_light;
uniform sampler2D image;
uniform vec3 tint;
out vec4 pixel;
void main() { pixel = texture(image, texture_uv) * vec4(tint * face_light, 1.0); }
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

bool Cube::create(std::string& error) {
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
    transform_ = glGetUniformLocation(program_, "transform");
    tint_ = glGetUniformLocation(program_, "tint");
    const GLint image = glGetUniformLocation(program_, "image");
    glGenVertexArrays(1, &array_);
    glBindVertexArray(array_);
    glGenBuffers(1, &vertices_);
    glBindBuffer(GL_ARRAY_BUFFER, vertices_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_data::vertices), cube_data::vertices.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &indices_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_data::indices), cube_data::indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(cube_data::Vertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(cube_data::Vertex),
                          reinterpret_cast<const void*>(offsetof(cube_data::Vertex, u)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(cube_data::Vertex),
                          reinterpret_cast<const void*>(offsetof(cube_data::Vertex, light)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    glGenTextures(1, &texture_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    const auto pixels = cube_data::checkerboard();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cube_data::texture_size, cube_data::texture_size,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    glUseProgram(program_);
    glUniform1i(image, 0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    if (glGetError() != GL_NO_ERROR || transform_ < 0 || tint_ < 0 || image < 0 ||
        !array_ || !vertices_ || !indices_ || !texture_) {
        error = "Cannot initialise cube buffers, texture or uniforms";
        destroy();
        return false;
    }
    return true;
}

void Cube::draw(int width, int height, int colour, float angle) {
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    glClearColor(0.035f, 0.05f, 0.075f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program_);
    const auto transform = cube_data::transform(angle, float(width) / float(height));
    glUniformMatrix4fv(transform_, 1, GL_FALSE, transform.data());
    constexpr float tints[][3] = {{1, 1, 1}, {1, 0.35f, 0.35f}, {0.35f, 1, 0.35f}};
    const auto& tint = tints[colour % 3];
    glUniform3fv(tint_, 1, tint);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glBindVertexArray(array_);
    glDrawElements(GL_TRIANGLES, cube_data::indices.size(), GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
}

void Cube::destroy() {
    if (vertices_) glDeleteBuffers(1, &vertices_);
    if (array_) glDeleteVertexArrays(1, &array_);
    if (program_) glDeleteProgram(program_);
    if (indices_) glDeleteBuffers(1, &indices_);
    if (texture_) glDeleteTextures(1, &texture_);
    vertices_ = array_ = program_ = indices_ = texture_ = 0;
}
