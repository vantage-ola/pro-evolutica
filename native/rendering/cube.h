#pragma once

#include <GLES3/gl3.h>
#include <string>
#include "../assets/png.h"
#include "../assets/mesh.h"
#include "camera.h"
#include "../assets/ktmdl.h"
#include "skinning.h"

#include <vector>

// All methods run on the thread with the current GL context.
class Cube {
public:
    bool create(std::string& error);
    bool set_texture(const assets::Image& image, std::string& error);
    bool add_instance_texture(const assets::Image& image, std::string& error);
    bool set_mesh(const assets::Mesh& mesh, std::string& error, bool dynamic = false);
    bool set_skinned_model(const assets::SkinnedModel& model, std::string& error);
    bool add_skinned_model(const assets::SkinnedModel& model, std::string& error);
    bool update_vertices(const std::vector<assets::MeshVertex>& vertices, std::string& error);
    void draw(int width, int height, int colour, float angle, const camera::View& view);
    void draw_skinned_instances(int width, int height, float angle, const camera::View& view,
                                const std::vector<skinning::Instance>& instances);
    void destroy();

private:
    GLuint program_ = 0;
    GLuint vertices_ = 0;
    GLuint array_ = 0;
    GLuint indices_ = 0;
    GLuint texture_ = 0;
    std::vector<GLuint> instance_textures_;
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
        GLint weight_count = 0;
    };
    struct SkinModel {
        assets::SkinnedModel model;
        std::vector<SkinDraw> draws;
    };
    std::vector<SkinModel> skin_models_;
    GLint skinned_ = -1;
    GLint skin_palette_ = -1, weight_count_ = -1;
    GLint instance_transform_ = -1;
};
