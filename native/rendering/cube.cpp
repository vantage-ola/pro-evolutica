#include "cube.h"
#include "cube_data.h"

#include <cstddef>
#include <cmath>

namespace {
constexpr const char* vertex_source = R"(#version 300 es
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;
uniform mat4 transform;
uniform mat4 rotation;
uniform bool mesh_mode;
uniform vec3 centre;
uniform float scale;
out vec2 texture_uv;
out float face_light;
void main() {
    gl_Position = transform * vec4((position - centre) * scale, 1.0);
    // The extracted mesh retains top-first source UVs; the original cube uses
    // bottom-first UVs. No image bytes are flipped or changed.
    texture_uv = mesh_mode ? uv : vec2(uv.x, 1.0 - uv.y);
    face_light = mesh_mode ? 0.3 + 0.7 * max(dot(mat3(rotation) * normalize(normal),
        normalize(vec3(0.4, 0.7, 1.0))), 0.0) : normal.x;
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
    rotation_ = glGetUniformLocation(program_, "rotation");
    mesh_mode_ = glGetUniformLocation(program_, "mesh_mode");
    centre_uniform_ = glGetUniformLocation(program_, "centre");
    scale_uniform_ = glGetUniformLocation(program_, "scale");
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
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    if (glGetError() != GL_NO_ERROR || transform_ < 0 || tint_ < 0 || image < 0 ||
        rotation_ < 0 || mesh_mode_ < 0 || centre_uniform_ < 0 || scale_uniform_ < 0 ||
        !array_ || !vertices_ || !indices_ || !texture_) {
        error = "Cannot initialise cube buffers, texture or uniforms";
        destroy();
        return false;
    }
    return true;
}

bool Cube::set_mesh(const assets::Mesh& mesh, std::string& error, bool dynamic) {
    if (mesh.vertices.empty() || mesh.vertices.size() > assets::max_mesh_vertices ||
        mesh.indices.empty() || mesh.indices.size() > assets::max_mesh_indices ||
        mesh.indices.size() % 3 || !std::isfinite(mesh.radius) || mesh.radius < 1e-6f) {
        error = "Invalid mesh counts or radius";
        return false;
    }
    for (const auto x : mesh.centre) if (!std::isfinite(x)) {
        error = "Invalid mesh centre";
        return false;
    }
    for (const auto index : mesh.indices) if (index >= mesh.vertices.size()) {
        error = "Invalid mesh index";
        return false;
    }
    GLuint array = 0, vertices = 0, indices = 0;
    glGenVertexArrays(1, &array);
    glBindVertexArray(array);
    glGenBuffers(1, &vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(assets::MeshVertex),
                 mesh.vertices.data(), dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    glGenBuffers(1, &indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(std::uint16_t),
                 mesh.indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(assets::MeshVertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(assets::MeshVertex),
                          reinterpret_cast<const void*>(offsetof(assets::MeshVertex, u)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(assets::MeshVertex),
                          reinterpret_cast<const void*>(offsetof(assets::MeshVertex, nx)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    const GLenum status = glGetError();
    if (!array || !vertices || !indices || status != GL_NO_ERROR) {
        while (glGetError() != GL_NO_ERROR) {}
        if (array) glDeleteVertexArrays(1, &array);
        if (vertices) glDeleteBuffers(1, &vertices);
        if (indices) glDeleteBuffers(1, &indices);
        error = "Cannot upload mesh; GL=" + std::to_string(status);
        return false;
    }
    glDeleteVertexArrays(1, &array_);
    glDeleteBuffers(1, &vertices_);
    glDeleteBuffers(1, &indices_);
    array_ = array; vertices_ = vertices; indices_ = indices;
    centre_ = mesh.centre;
    scale_ = 1.0f / mesh.radius;
    index_count_ = static_cast<GLsizei>(mesh.indices.size());
    mesh_ = true;
    vertex_count_ = mesh.vertices.size();
    return true;
}

bool Cube::update_vertices(const std::vector<assets::MeshVertex>& vertices, std::string& error) {
    if (!mesh_ || vertices.size()!=vertex_count_) {
        error="Animation vertex count changed";
        return false;
    }
    glBindBuffer(GL_ARRAY_BUFFER,vertices_);
    glBufferSubData(GL_ARRAY_BUFFER,0,vertices.size()*sizeof(assets::MeshVertex),vertices.data());
    const auto status=glGetError();
    if (status!=GL_NO_ERROR) {
        error="Cannot update animation vertices; GL="+std::to_string(status);
        return false;
    }
    return true;
}

bool Cube::set_texture(const assets::Image& image, std::string& error) {
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (maximum <= 0 || !image.width || !image.height || image.width > assets::max_image_dimension ||
        image.height > assets::max_image_dimension ||
        std::uint64_t(image.width) * image.height > assets::max_image_pixels ||
        image.width > static_cast<std::uint32_t>(maximum) ||
        image.height > static_cast<std::uint32_t>(maximum) ||
        image.rgba.size() != std::size_t(image.width) * image.height * 4) {
        error = "Texture dimensions or RGBA byte count are invalid";
        return false;
    }
    GLuint replacement = 0;
    glGenTextures(1, &replacement);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, replacement);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    const GLenum status = glGetError();
    if (!replacement || status != GL_NO_ERROR) {
        // A failed allocation can also make mipmap generation fail. Clear all
        // upload errors so the preserved checkerboard can continue drawing.
        while (glGetError() != GL_NO_ERROR) {}
        if (replacement) glDeleteTextures(1, &replacement);
        glBindTexture(GL_TEXTURE_2D, texture_);
        error = "Cannot upload RGBA texture; GL=" + std::to_string(status);
        return false;
    }
    glDeleteTextures(1, &texture_);
    texture_ = replacement;
    return true;
}

void Cube::draw(int width, int height, int colour, float angle, const camera::View& view) {
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    glClearColor(0.035f, 0.05f, 0.075f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program_);
    const auto transform = view.transform(angle, float(width) / float(height), mesh_ ? 1.f : std::sqrt(3.f));
    glUniformMatrix4fv(transform_, 1, GL_FALSE, transform.data());
    const auto rotation = cube_data::rotation(angle);
    glUniformMatrix4fv(rotation_, 1, GL_FALSE, rotation.data());
    glUniform1i(mesh_mode_, mesh_);
    glUniform3fv(centre_uniform_, 1, centre_.data());
    glUniform1f(scale_uniform_, scale_);
    constexpr float tints[][3] = {{1, 1, 1}, {1, 0.35f, 0.35f}, {0.35f, 1, 0.35f}};
    const auto& tint = tints[colour % 3];
    glUniform3fv(tint_, 1, tint);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glBindVertexArray(array_);
    glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
}

void Cube::destroy() {
    if (vertices_) glDeleteBuffers(1, &vertices_);
    if (array_) glDeleteVertexArrays(1, &array_);
    if (program_) glDeleteProgram(program_);
    if (indices_) glDeleteBuffers(1, &indices_);
    if (texture_) glDeleteTextures(1, &texture_);
    vertices_ = array_ = program_ = indices_ = texture_ = 0;
    centre_ = {};
    scale_ = 1;
    mesh_ = false;
    vertex_count_ = 0;
    index_count_ = 36;
}
