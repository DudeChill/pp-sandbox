#pragma once

#include <cstddef>

#if defined(_WIN32)
#  define PP_GLAPI __stdcall
#else
#  define PP_GLAPI
#endif

namespace glapi {

using GLuint = unsigned int;
using GLint = int;
using GLenum = unsigned int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;
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

extern GLuint (PP_GLAPI *CreateShader)(GLenum);
extern void (PP_GLAPI *ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
extern void (PP_GLAPI *CompileShader)(GLuint);
extern void (PP_GLAPI *GetShaderiv)(GLuint, GLenum, GLint*);
extern void (PP_GLAPI *GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (PP_GLAPI *DeleteShader)(GLuint);
extern GLuint (PP_GLAPI *CreateProgram)();
extern void (PP_GLAPI *AttachShader)(GLuint, GLuint);
extern void (PP_GLAPI *LinkProgram)(GLuint);
extern void (PP_GLAPI *GetProgramiv)(GLuint, GLenum, GLint*);
extern void (PP_GLAPI *GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (PP_GLAPI *DeleteProgram)(GLuint);
extern void (PP_GLAPI *UseProgram)(GLuint);
extern void (PP_GLAPI *GenVertexArrays)(GLsizei, GLuint*);
extern void (PP_GLAPI *BindVertexArray)(GLuint);
extern void (PP_GLAPI *DeleteVertexArrays)(GLsizei, const GLuint*);
extern void (PP_GLAPI *GenBuffers)(GLsizei, GLuint*);
extern void (PP_GLAPI *BindBuffer)(GLenum, GLuint);
extern void (PP_GLAPI *BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
extern void (PP_GLAPI *BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*);
extern void (PP_GLAPI *DeleteBuffers)(GLsizei, const GLuint*);
extern void (PP_GLAPI *EnableVertexAttribArray)(GLuint);
extern void (PP_GLAPI *VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
extern void (PP_GLAPI *VertexAttribDivisor)(GLuint, GLuint);
extern GLint (PP_GLAPI *GetUniformLocation)(GLuint, const GLchar*);
extern void (PP_GLAPI *UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
extern void (PP_GLAPI *Enable)(GLenum);
extern void (PP_GLAPI *Disable)(GLenum);
extern void (PP_GLAPI *BlendFunc)(GLenum, GLenum);
extern void (PP_GLAPI *ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
extern void (PP_GLAPI *Clear)(GLenum);
extern void (PP_GLAPI *Viewport)(GLint, GLint, GLsizei, GLsizei);
extern void (PP_GLAPI *DrawArraysInstanced)(GLenum, GLint, GLsizei, GLsizei);
extern const unsigned char* (PP_GLAPI *GetString)(GLenum);

bool load();

} // namespace glapi
