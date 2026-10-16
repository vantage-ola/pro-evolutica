#pragma once

#include <GLES3/gl3.h>
#include <string>
#include "../assets/png.h"
#include "../assets/mesh.h"
#include "camera.h"
#include "../assets/ktmdl.h"

#include <vector>

// All methods run on the thread with the current GL context.
class Cube {
public:
    bool create(std::string& error);
    bool set_texture(const assets::Image& image, std::string& error);
    bool set_mesh(const assets::Mesh& mesh, std::string& error, bool dynamic = false);
    bool set_skinned_model(const assets::SkinnedModel& model, std::string& error);
    bool update_vertices(const std::vector<assets::MeshVertex>& vertices, std::string& error);
    void draw(int width, int height, int colour, float angle, const camera::View& view);
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
    std::size_t vertex_count_ = 0;
    GLsizei index_count_ = 36;
    struct SkinDraw {
        GLuint array = 0, vertices = 0, indices = 0;
        GLsizei index_count = 0;
        std::vector<std::uint16_t> palette;
        bool has_weight = false;
    };
    std::vector<SkinDraw> skin_draws_;
    GLint skinned_ = -1;
    GLint skin_palette_ = -1, has_weight_ = -1;
};
