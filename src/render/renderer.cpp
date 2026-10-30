#include "renderer.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "gl_api.h"

namespace render {
namespace {

const char* kVertex = R"GLSL(
#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec2 i_pos;
layout(location = 3) in vec2 i_size;
layout(location = 4) in float i_angle;
layout(location = 5) in vec4 i_color;
layout(location = 6) in float i_shape;

uniform mat4 u_vp;
out vec2 v_uv;
out vec4 v_color;
out float v_shape;

void main() {
    float c = cos(i_angle);
    float s = sin(i_angle);
    vec2 local = vec2(c * a_pos.x - s * a_pos.y, s * a_pos.x + c * a_pos.y);
    vec2 world = local * i_size + i_pos;
    gl_Position = u_vp * vec4(world, 0.0, 1.0);
    v_uv = a_uv - 0.5;
    v_color = i_color;
    v_shape = i_shape;
}
)GLSL";

const char* kFragment = R"GLSL(
#version 330 core
in vec2 v_uv;
in vec4 v_color;
in float v_shape;
out vec4 out_color;

void main() {
    vec4 c = v_color;
    if (v_shape > 0.5) {
        float d = length(v_uv);
        if (d > 0.5) discard;
        c.rgb *= mix(0.62, 1.0, smoothstep(0.34, 0.5, d));
        out_color = c;
    } else {
        vec2 e = vec2(0.5) - abs(v_uv);
        float edge = min(e.x, e.y) * 2.0;
        c.rgb *= mix(0.55, 1.0, smoothstep(0.0, 0.06, edge));
        out_color = c;
    }
}
)GLSL";

unsigned compile(unsigned type, const char* source) {
    const unsigned shader = glapi::CreateShader(type);
    glapi::ShaderSource(shader, 1, &source, nullptr);
    glapi::CompileShader(shader);
    int ok = 0;
    glapi::GetShaderiv(shader, glapi::kCompileStatus, &ok);
    if (!ok) {
        char log[2048];
        int len = 0;
        glapi::GetShaderInfoLog(shader, sizeof log, &len, log);
        std::fprintf(stderr, "shader compile failed:\n%.*s\n", len, log);
        glapi::DeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

bool Renderer::init() {
    const unsigned vs = compile(glapi::kVertexShader, kVertex);
    const unsigned fs = compile(glapi::kFragmentShader, kFragment);
    if (vs == 0 || fs == 0) return false;

    program_ = glapi::CreateProgram();
    glapi::AttachShader(program_, vs);
    glapi::AttachShader(program_, fs);
    glapi::LinkProgram(program_);
    glapi::DeleteShader(vs);
    glapi::DeleteShader(fs);

    int ok = 0;
    glapi::GetProgramiv(program_, glapi::kLinkStatus, &ok);
    if (!ok) {
        char log[2048];
        int len = 0;
        glapi::GetProgramInfoLog(program_, sizeof log, &len, log);
        std::fprintf(stderr, "program link failed:\n%.*s\n", len, log);
        return false;
    }
    uniform_vp_ = glapi::GetUniformLocation(program_, "u_vp");

    const float quad[] = {
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
    };

    glapi::GenVertexArrays(1, &vao_);
    glapi::BindVertexArray(vao_);

    glapi::GenBuffers(1, &quad_vbo_);
    glapi::BindBuffer(glapi::kArrayBuffer, quad_vbo_);
    glapi::BufferData(glapi::kArrayBuffer, sizeof quad, quad, glapi::kStaticDraw);

    glapi::GenBuffers(1, &inst_vbo_);
    glapi::BindBuffer(glapi::kArrayBuffer, inst_vbo_);
    instance_capacity_ = 16384;
    glapi::BufferData(glapi::kArrayBuffer,
                      static_cast<glapi::GLsizeiptr>(instance_capacity_ * sizeof(Instance)),
                      nullptr, glapi::kDynamicDraw);

    glapi::BindBuffer(glapi::kArrayBuffer, quad_vbo_);
    glapi::EnableVertexAttribArray(0);
    glapi::VertexAttribPointer(0, 2, glapi::kFloat, glapi::kFalse, 4 * sizeof(float), nullptr);
    glapi::EnableVertexAttribArray(1);
    glapi::VertexAttribPointer(1, 2, glapi::kFloat, glapi::kFalse, 4 * sizeof(float),
                               reinterpret_cast<const void*>(2 * sizeof(float)));

    glapi::BindBuffer(glapi::kArrayBuffer, inst_vbo_);
    const auto base = 10 * sizeof(float);
    glapi::EnableVertexAttribArray(2);
    glapi::VertexAttribPointer(2, 2, glapi::kFloat, glapi::kFalse,
                               static_cast<glapi::GLsizei>(base), nullptr);
    glapi::VertexAttribDivisor(2, 1);
    glapi::EnableVertexAttribArray(3);
    glapi::VertexAttribPointer(3, 2, glapi::kFloat, glapi::kFalse,
                               static_cast<glapi::GLsizei>(base),
                               reinterpret_cast<const void*>(2 * sizeof(float)));
    glapi::VertexAttribDivisor(3, 1);
    glapi::EnableVertexAttribArray(4);
    glapi::VertexAttribPointer(4, 1, glapi::kFloat, glapi::kFalse,
                               static_cast<glapi::GLsizei>(base),
                               reinterpret_cast<const void*>(4 * sizeof(float)));
    glapi::VertexAttribDivisor(4, 1);
    glapi::EnableVertexAttribArray(5);
    glapi::VertexAttribPointer(5, 4, glapi::kFloat, glapi::kFalse,
                               static_cast<glapi::GLsizei>(base),
                               reinterpret_cast<const void*>(5 * sizeof(float)));
    glapi::VertexAttribDivisor(5, 1);
    glapi::EnableVertexAttribArray(6);
    glapi::VertexAttribPointer(6, 1, glapi::kFloat, glapi::kFalse,
                               static_cast<glapi::GLsizei>(base),
                               reinterpret_cast<const void*>(9 * sizeof(float)));
    glapi::VertexAttribDivisor(6, 1);

    instances_.reserve(static_cast<size_t>(instance_capacity_));
    glapi::Enable(glapi::kBlend);
    glapi::BlendFunc(glapi::kSrcAlpha, glapi::kOneMinusSrcAlpha);
    glapi::Disable(glapi::kDepthTest);
    glapi::ClearColor(0.07f, 0.08f, 0.10f, 1.0f);
    return true;
}

void Renderer::shutdown() {
    if (program_ != 0) glapi::DeleteProgram(program_);
    if (quad_vbo_ != 0) glapi::DeleteBuffers(1, &quad_vbo_);
    if (inst_vbo_ != 0) glapi::DeleteBuffers(1, &inst_vbo_);
    if (vao_ != 0) glapi::DeleteVertexArrays(1, &vao_);
    program_ = 0;
}

void Renderer::begin(int width, int height, sim::Vec2 center, float view_height) {
    width_ = width;
    height_ = height;
    instances_.clear();

    const float aspect = static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1);
    const float half_h = view_height * 0.5f;
    const float half_w = half_h * aspect;
    const float left = center.x - half_w;
    const float right = center.x + half_w;
    const float bottom = center.y - half_h;
    const float top = center.y + half_h;

    const float m[16] = {
        2.0f / (right - left), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -(right + left) / (right - left), -(top + bottom) / (top - bottom), 0.0f, 1.0f,
    };

    glapi::Viewport(0, 0, width, height);
    glapi::Clear(glapi::kColorBufferBit);
    glapi::UseProgram(program_);
    glapi::UniformMatrix4fv(uniform_vp_, 1, glapi::kFalse, m);
    glapi::BindVertexArray(vao_);
}

void Renderer::box(sim::Vec2 center, sim::Vec2 half, float angle, Color color) {
    if (instances_.size() >= instances_.capacity()) return;
    instances_.push_back({center.x, center.y, half.x * 2.0f, half.y * 2.0f, angle,
                          color.r, color.g, color.b, color.a, 0.0f});
}

void Renderer::circle(sim::Vec2 center, float radius, Color color) {
    if (instances_.size() >= instances_.capacity()) return;
    instances_.push_back({center.x, center.y, radius * 2.0f, radius * 2.0f, 0.0f,
                          color.r, color.g, color.b, color.a, 1.0f});
}

void Renderer::line(sim::Vec2 a, sim::Vec2 b, float thickness, Color color) {
    const sim::Vec2 d = b - a;
    const float len = sim::length(d);
    if (len < 1e-6f) return;
    box((a + b) * 0.5f, sim::Vec2{len * 0.5f, thickness * 0.5f},
        std::atan2(d.y, d.x), color);
}

void Renderer::end() {
    if (instances_.empty()) return;
    glapi::BindBuffer(glapi::kArrayBuffer, inst_vbo_);
    glapi::BufferSubData(glapi::kArrayBuffer, 0,
                         static_cast<glapi::GLsizeiptr>(instances_.size() * sizeof(Instance)),
                         instances_.data());
    glapi::DrawArraysInstanced(glapi::kTriangles, 0, 6,
                               static_cast<glapi::GLsizei>(instances_.size()));
}

} // namespace render
