#include "gl_api.h"

#include <cstdio>

#include "platform/sdl.h"

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

GLuint (PP_GLAPI *CreateShader)(GLenum) = nullptr;
void (PP_GLAPI *ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
void (PP_GLAPI *CompileShader)(GLuint) = nullptr;
void (PP_GLAPI *GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
void (PP_GLAPI *GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (PP_GLAPI *DeleteShader)(GLuint) = nullptr;
GLuint (PP_GLAPI *CreateProgram)() = nullptr;
void (PP_GLAPI *AttachShader)(GLuint, GLuint) = nullptr;
void (PP_GLAPI *LinkProgram)(GLuint) = nullptr;
void (PP_GLAPI *GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
void (PP_GLAPI *GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (PP_GLAPI *DeleteProgram)(GLuint) = nullptr;
void (PP_GLAPI *UseProgram)(GLuint) = nullptr;
void (PP_GLAPI *GenVertexArrays)(GLsizei, GLuint*) = nullptr;
void (PP_GLAPI *BindVertexArray)(GLuint) = nullptr;
void (PP_GLAPI *DeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
void (PP_GLAPI *GenBuffers)(GLsizei, GLuint*) = nullptr;
void (PP_GLAPI *BindBuffer)(GLenum, GLuint) = nullptr;
void (PP_GLAPI *BufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
void (PP_GLAPI *BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*) = nullptr;
void (PP_GLAPI *DeleteBuffers)(GLsizei, const GLuint*) = nullptr;
void (PP_GLAPI *EnableVertexAttribArray)(GLuint) = nullptr;
void (PP_GLAPI *VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
void (PP_GLAPI *VertexAttribDivisor)(GLuint, GLuint) = nullptr;
GLint (PP_GLAPI *GetUniformLocation)(GLuint, const GLchar*) = nullptr;
void (PP_GLAPI *UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;
void (PP_GLAPI *Enable)(GLenum) = nullptr;
void (PP_GLAPI *Disable)(GLenum) = nullptr;
void (PP_GLAPI *BlendFunc)(GLenum, GLenum) = nullptr;
void (PP_GLAPI *ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (PP_GLAPI *Clear)(GLenum) = nullptr;
void (PP_GLAPI *Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
void (PP_GLAPI *DrawArraysInstanced)(GLenum, GLint, GLsizei, GLsizei) = nullptr;
const unsigned char* (PP_GLAPI *GetString)(GLenum) = nullptr;

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
