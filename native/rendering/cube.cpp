#include "cube.h"
#include "cube_data.h"

#include <cstddef>
#include <cmath>

namespace {
constexpr const char* vertex_source = R"(#version 300 es
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;
layout(location = 3) in uvec4 bone_indices;
layout(location = 4) in float bone_weight;
uniform mat4 transform;
uniform mat4 rotation;
uniform bool mesh_mode;
uniform bool skinned;
uniform bool has_weight;
uniform mat4 skin_palette[32];
uniform vec3 centre;
uniform float scale;
out vec2 texture_uv;
out float face_light;
void main() {
    vec4 source_position = vec4(position, 1.0);
    vec3 source_normal = normal;
    if (skinned) {
        vec4 first = skin_palette[bone_indices.x] * source_position;
        if (has_weight) {
            vec4 second = skin_palette[bone_indices.y] * source_position;
            source_position = first * bone_weight + second * (1.0 - bone_weight);
            source_normal = mat3(skin_palette[bone_indices.x]) * normal * bone_weight +
                            mat3(skin_palette[bone_indices.y]) * normal * (1.0 - bone_weight);
        } else {
            source_position = first;
            source_normal = mat3(skin_palette[bone_indices.x]) * normal;
        }
    }
    gl_Position = transform * vec4((source_position.xyz - centre) * scale, 1.0);
    // The extracted mesh retains top-first source UVs; the original cube uses
    // bottom-first UVs. No image bytes are flipped or changed.
    texture_uv = mesh_mode ? uv : vec2(uv.x, 1.0 - uv.y);
    face_light = mesh_mode ? 0.3 + 0.7 * max(dot(mat3(rotation) * normalize(source_normal),
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
    skinned_ = glGetUniformLocation(program_, "skinned");
    skin_palette_ = glGetUniformLocation(program_, "skin_palette[0]");
    has_weight_ = glGetUniformLocation(program_, "has_weight");
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
        skinned_ < 0 || skin_palette_ < 0 || has_weight_ < 0 ||
        !array_ || !vertices_ || !indices_ || !texture_) {
        error = "Cannot initialise cube buffers, texture or uniforms";
        destroy();
        return false;
    }
    return true;
}

bool Cube::set_skinned_model(const assets::SkinnedModel& model, std::string& error) {
    if (model.bones.empty() || model.bones.size() > 32 || model.packets.empty()) {
        error = "Skinned model exceeds renderer bone/packet limits";
        return false;
    }
    std::vector<SkinDraw> replacement;
    replacement.reserve(model.packets.size());
    std::array<float, 3> lower{}, upper{};
    bool have_position = false;
    try {
        for (const auto& packet : model.packets) {
            if (packet.vertex_count == 0 || packet.vertex_stride == 0 ||
                packet.vertices.size() != std::size_t(packet.vertex_count) * packet.vertex_stride ||
                packet.indices.empty() || packet.indices.size() % 3 || packet.palette.empty() ||
                packet.palette.size() > 32) throw std::runtime_error("Invalid skinned packet data");
            const assets::SkinAttribute *position = nullptr, *bone_ids = nullptr, *weight = nullptr,
                *normal = nullptr, *uv = nullptr;
            for (const auto& attribute : packet.attributes) {
                if (attribute.semantic == 16 && attribute.format == 2) position = &attribute;
                if (attribute.semantic == 18 && attribute.format == 2) normal = &attribute;
                if (attribute.semantic == 22 && attribute.format == 1) uv = &attribute;
                if (attribute.semantic == 32 && attribute.format == 0) weight = &attribute;
                if (attribute.semantic == 33 && attribute.format == 11) bone_ids = &attribute;
            }
            if (!position || !normal || !uv || !bone_ids)
                throw std::runtime_error("Skinned packet lacks renderer attributes");
            for (const auto bone : packet.palette)
                if (bone >= model.bones.size()) throw std::runtime_error("Packet palette exceeds skeleton");
            for (const auto index : packet.indices)
                if (index >= packet.vertex_count) throw std::runtime_error("Packet index exceeds vertices");
            if (weight) {
                for (std::size_t i = 0; i < packet.vertex_count; ++i) {
                    const auto* value = reinterpret_cast<const float*>(packet.vertices.data() +
                        i * packet.vertex_stride + weight->offset);
                    if (!std::isfinite(*value) || *value < 0.f || *value > 1.f)
                        throw std::runtime_error("Skinned blend weight is outside 0..1");
                }
            }
            for (std::size_t i = 0; i < packet.vertex_count; ++i) {
                const auto* ids = packet.vertices.data() + i * packet.vertex_stride + bone_ids->offset;
                const std::size_t used = weight ? 2 : 1;
                for (std::size_t j = 0; j < used; ++j)
                    if (ids[j] >= packet.palette.size())
                        throw std::runtime_error("Vertex bone index exceeds packet palette");
            }
            for (std::size_t i = 0; i < packet.vertex_count; ++i) {
                const auto* value = reinterpret_cast<const float*>(packet.vertices.data() +
                    i * packet.vertex_stride + position->offset);
                for (int c = 0; c < 3; ++c) {
                    if (!std::isfinite(value[c]) || std::abs(value[c]) > 1'000'000.f)
                        throw std::runtime_error("Invalid skinned vertex position");
                    if (!have_position) lower[c] = upper[c] = value[c];
                    else { lower[c] = std::min(lower[c], value[c]); upper[c] = std::max(upper[c], value[c]); }
                }
                have_position = true;
            }
            SkinDraw draw;
            draw.palette = packet.palette;
            draw.index_count = static_cast<GLsizei>(packet.indices.size());
            draw.has_weight = weight != nullptr;
            glGenVertexArrays(1, &draw.array);
            glBindVertexArray(draw.array);
            glGenBuffers(1, &draw.vertices);
            glBindBuffer(GL_ARRAY_BUFFER, draw.vertices);
            glBufferData(GL_ARRAY_BUFFER, packet.vertices.size(), packet.vertices.data(), GL_STATIC_DRAW);
            glGenBuffers(1, &draw.indices);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw.indices);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, packet.indices.size() * sizeof(std::uint16_t),
                         packet.indices.data(), GL_STATIC_DRAW);
            const auto pointer = [](std::uint8_t offset) { return reinterpret_cast<const void*>(std::uintptr_t(offset)); };
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, packet.vertex_stride, pointer(position->offset));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, packet.vertex_stride, pointer(uv->offset));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, packet.vertex_stride, pointer(normal->offset));
            glEnableVertexAttribArray(2);
            glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, packet.vertex_stride, pointer(bone_ids->offset));
            glEnableVertexAttribArray(3);
            if (weight) {
                glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, packet.vertex_stride, pointer(weight->offset));
                glEnableVertexAttribArray(4);
            } else {
                glDisableVertexAttribArray(4);
                glVertexAttrib1f(4, 1.f);
            }
            replacement.push_back(draw);
            if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL rejected skinned packet upload");
        }
    } catch (const std::exception& e) {
        for (const auto& draw : replacement) {
            if (draw.array) glDeleteVertexArrays(1, &draw.array);
            if (draw.vertices) glDeleteBuffers(1, &draw.vertices);
            if (draw.indices) glDeleteBuffers(1, &draw.indices);
        }
        error = e.what();
        return false;
    }
    for (const auto& draw : skin_draws_) {
        if (draw.array) glDeleteVertexArrays(1, &draw.array);
        if (draw.vertices) glDeleteBuffers(1, &draw.vertices);
        if (draw.indices) glDeleteBuffers(1, &draw.indices);
    }
    skin_draws_ = std::move(replacement);
    centre_ = {(lower[0]+upper[0])*.5f, (lower[1]+upper[1])*.5f, (lower[2]+upper[2])*.5f};
    double radius = 0;
    for (const auto& packet : model.packets) {
        const auto position = std::find_if(packet.attributes.begin(), packet.attributes.end(),
            [](const auto& a) { return a.semantic == 16 && a.format == 2; });
        for (std::size_t i = 0; i < packet.vertex_count; ++i) {
            const auto* value = reinterpret_cast<const float*>(packet.vertices.data() +
                i * packet.vertex_stride + position->offset);
            radius = std::max(radius, std::hypot(double(value[0]-centre_[0]),
                double(value[1]-centre_[1]), double(value[2]-centre_[2])));
        }
    }
    if (!(radius > 1e-6)) {
        error = "Skinned model has negligible bounds";
        return false;
    }
    scale_ = 1.f / static_cast<float>(radius);
    mesh_ = true;
    vertex_count_ = 0;
    index_count_ = 0;
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
    for (const auto& draw : skin_draws_) {
        if (draw.array) glDeleteVertexArrays(1, &draw.array);
        if (draw.vertices) glDeleteBuffers(1, &draw.vertices);
        if (draw.indices) glDeleteBuffers(1, &draw.indices);
    }
    skin_draws_.clear();
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
    if (!skin_draws_.empty()) {
        constexpr std::array<float, 32 * 16> identity = [] {
            std::array<float, 32 * 16> matrices{};
            for (std::size_t m = 0; m < 32; ++m)
                for (std::size_t i = 0; i < 4; ++i) matrices[m * 16 + i * 5] = 1.f;
            return matrices;
        }();
        glUniform1i(skinned_, GL_TRUE);
        // The game model's original winding comes from D3D draw packets. Keep
        // both sides visible until the Android front-face convention is verified.
        glDisable(GL_CULL_FACE);
        for (const auto& packet : skin_draws_) {
            glUniform1i(has_weight_, packet.has_weight ? GL_TRUE : GL_FALSE);
            glUniformMatrix4fv(skin_palette_, static_cast<GLsizei>(packet.palette.size()), GL_FALSE,
                               identity.data());
            glBindVertexArray(packet.array);
            glDrawElements(GL_TRIANGLES, packet.index_count, GL_UNSIGNED_SHORT, nullptr);
        }
        glEnable(GL_CULL_FACE);
        glUniform1i(skinned_, GL_FALSE);
    } else {
        glUniform1i(skinned_, GL_FALSE);
        glBindVertexArray(array_);
        glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_SHORT, nullptr);
    }
    glBindVertexArray(0);
}

void Cube::destroy() {
    for (const auto& draw : skin_draws_) {
        if (draw.array) glDeleteVertexArrays(1, &draw.array);
        if (draw.vertices) glDeleteBuffers(1, &draw.vertices);
        if (draw.indices) glDeleteBuffers(1, &draw.indices);
    }
    skin_draws_.clear();
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
