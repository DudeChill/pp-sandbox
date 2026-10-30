#pragma once

namespace glapi {

using GLuint = unsigned int;
using GLint = int;
using GLenum = unsigned int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLchar = char;
using GLsizeiptr = long;
using GLintptr = long;
using GLfloat = float;

constexpr GLenum kVertexShader = 0x8B31;
constexpr GLenum kFragmentShader = 0x8B30;
constexpr GLenum kCompileStatus = 0x8B81;
constexpr GLenum kLinkStatus = 0x8B82;
constexpr GLenum kInfoLogLength = 0x8B84;
constexpr GLenum kArrayBuffer = 0x8892;
constexpr GLenum kDynamicDraw = 0x88E8;
constexpr GLenum kStaticDraw = 0x88E4;
constexpr GLenum kFloat = 0x1406;
constexpr GLenum kTriangles = 0x0004;
constexpr GLenum kBlend = 0x0BE2;
constexpr GLenum kSrcAlpha = 0x0302;
constexpr GLenum kOneMinusSrcAlpha = 0x0303;
constexpr GLenum kColorBufferBit = 0x4000;
constexpr GLenum kDepthTest = 0x0B71;
constexpr GLenum kMultisample = 0x809D;
constexpr GLboolean kFalse = 0;
constexpr GLboolean kTrue = 1;

extern GLuint (*CreateShader)(GLenum);
extern void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
extern void (*CompileShader)(GLuint);
extern void (*GetShaderiv)(GLuint, GLenum, GLint*);
extern void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*DeleteShader)(GLuint);
extern GLuint (*CreateProgram)();
extern void (*AttachShader)(GLuint, GLuint);
extern void (*LinkProgram)(GLuint);
extern void (*GetProgramiv)(GLuint, GLenum, GLint*);
extern void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*DeleteProgram)(GLuint);
extern void (*UseProgram)(GLuint);
extern void (*GenVertexArrays)(GLsizei, GLuint*);
extern void (*BindVertexArray)(GLuint);
extern void (*DeleteVertexArrays)(GLsizei, const GLuint*);
extern void (*GenBuffers)(GLsizei, GLuint*);
extern void (*BindBuffer)(GLenum, GLuint);
extern void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
extern void (*BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*);
extern void (*DeleteBuffers)(GLsizei, const GLuint*);
extern void (*EnableVertexAttribArray)(GLuint);
extern void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
extern void (*VertexAttribDivisor)(GLuint, GLuint);
extern GLint (*GetUniformLocation)(GLuint, const GLchar*);
extern void (*UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
extern void (*Enable)(GLenum);
extern void (*Disable)(GLenum);
extern void (*BlendFunc)(GLenum, GLenum);
extern void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
extern void (*Clear)(GLenum);
extern void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
extern void (*DrawArraysInstanced)(GLenum, GLint, GLsizei, GLsizei);
extern const unsigned char* (*GetString)(GLenum);

bool load();

} // namespace glapi
