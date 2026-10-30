#include "gl_api.h"

#include <cstdio>

#include <SDL2/SDL.h>

namespace glapi {
namespace {

template <class T>
bool load_fn(T& fn, const char* name) {
    fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    if (!fn) {
        std::fprintf(stderr, "gl: missing %s\n", name);
        return false;
    }
    return true;
}

} // namespace

GLuint (*CreateShader)(GLenum) = nullptr;
void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
void (*CompileShader)(GLuint) = nullptr;
void (*GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*DeleteShader)(GLuint) = nullptr;
GLuint (*CreateProgram)() = nullptr;
void (*AttachShader)(GLuint, GLuint) = nullptr;
void (*LinkProgram)(GLuint) = nullptr;
void (*GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*DeleteProgram)(GLuint) = nullptr;
void (*UseProgram)(GLuint) = nullptr;
void (*GenVertexArrays)(GLsizei, GLuint*) = nullptr;
void (*BindVertexArray)(GLuint) = nullptr;
void (*DeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
void (*GenBuffers)(GLsizei, GLuint*) = nullptr;
void (*BindBuffer)(GLenum, GLuint) = nullptr;
void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
void (*BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*) = nullptr;
void (*DeleteBuffers)(GLsizei, const GLuint*) = nullptr;
void (*EnableVertexAttribArray)(GLuint) = nullptr;
void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
void (*VertexAttribDivisor)(GLuint, GLuint) = nullptr;
GLint (*GetUniformLocation)(GLuint, const GLchar*) = nullptr;
void (*UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;
void (*Enable)(GLenum) = nullptr;
void (*Disable)(GLenum) = nullptr;
void (*BlendFunc)(GLenum, GLenum) = nullptr;
void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*Clear)(GLenum) = nullptr;
void (*Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
void (*DrawArraysInstanced)(GLenum, GLint, GLsizei, GLsizei) = nullptr;
const unsigned char* (*GetString)(GLenum) = nullptr;

bool load() {
    bool ok = true;
    ok &= load_fn(CreateShader, "glCreateShader");
    ok &= load_fn(ShaderSource, "glShaderSource");
    ok &= load_fn(CompileShader, "glCompileShader");
    ok &= load_fn(GetShaderiv, "glGetShaderiv");
    ok &= load_fn(GetShaderInfoLog, "glGetShaderInfoLog");
    ok &= load_fn(DeleteShader, "glDeleteShader");
    ok &= load_fn(CreateProgram, "glCreateProgram");
    ok &= load_fn(AttachShader, "glAttachShader");
    ok &= load_fn(LinkProgram, "glLinkProgram");
    ok &= load_fn(GetProgramiv, "glGetProgramiv");
    ok &= load_fn(GetProgramInfoLog, "glGetProgramInfoLog");
    ok &= load_fn(DeleteProgram, "glDeleteProgram");
    ok &= load_fn(UseProgram, "glUseProgram");
    ok &= load_fn(GenVertexArrays, "glGenVertexArrays");
    ok &= load_fn(BindVertexArray, "glBindVertexArray");
    ok &= load_fn(DeleteVertexArrays, "glDeleteVertexArrays");
    ok &= load_fn(GenBuffers, "glGenBuffers");
    ok &= load_fn(BindBuffer, "glBindBuffer");
    ok &= load_fn(BufferData, "glBufferData");
    ok &= load_fn(BufferSubData, "glBufferSubData");
    ok &= load_fn(DeleteBuffers, "glDeleteBuffers");
    ok &= load_fn(EnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= load_fn(VertexAttribPointer, "glVertexAttribPointer");
    ok &= load_fn(VertexAttribDivisor, "glVertexAttribDivisor");
    ok &= load_fn(GetUniformLocation, "glGetUniformLocation");
    ok &= load_fn(UniformMatrix4fv, "glUniformMatrix4fv");
    ok &= load_fn(Enable, "glEnable");
    ok &= load_fn(Disable, "glDisable");
    ok &= load_fn(BlendFunc, "glBlendFunc");
    ok &= load_fn(ClearColor, "glClearColor");
    ok &= load_fn(Clear, "glClear");
    ok &= load_fn(Viewport, "glViewport");
    ok &= load_fn(DrawArraysInstanced, "glDrawArraysInstanced");
    ok &= load_fn(GetString, "glGetString");
    return ok;
}

} // namespace glapi
