#include <png.h>
#include <EGL/egl.h>
#include <GLES3/gl32.h>

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

const uint32_t kWidth = 255;
const uint32_t kHeight = 255;
const std::string kVertexShaderPath = "indexed_bug.vert";
const std::string kFragmentShaderPath = "indexed_bug.frag";

struct Vertex {
  float position[3];
  float color[3];
};

EGLDisplay display;
EGLContext context;

#define ERROR(message) \
    std::cerr << message << std::endl; \
    std::exit(EXIT_FAILURE)

void InitializeEGL() {
  display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(display, nullptr, nullptr);
  eglBindAPI(EGL_OPENGL_ES_API);

  const EGLint config_attribs[] = {
      EGL_SURFACE_TYPE, EGL_DONT_CARE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE };
  EGLConfig egl_config;
  EGLint num_configs;
  if (!eglChooseConfig(display, config_attribs, &egl_config, 1, &num_configs)) {
    ERROR("Unable to choose EGL config");
  }

  const EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  context = eglCreateContext(display, egl_config, EGL_NO_CONTEXT, context_attribs);
  if (context == EGL_NO_CONTEXT) {
    ERROR("Failed to create GLES context");
  }

  if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
    ERROR("Failed to make GLES context current");
  }
}

void SetUpFramebuffer() {
  GLuint color_buffer;
  glGenRenderbuffers(1, &color_buffer);
  glBindRenderbuffer(GL_RENDERBUFFER, color_buffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kWidth, kHeight);

  GLuint framebuffer;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_buffer);
}

GLuint CompileShader(GLenum type, std::string path) {
  std::ifstream fstream(path);
  if (!fstream) {
    ERROR("Unable to read shader: " << path);
  }
  std::stringstream buffer;
  buffer << fstream.rdbuf();
  std::string shader_source_tmp = buffer.str();
  const char* shader_source = shader_source_tmp.c_str();

  GLuint shader = glCreateShader(type);
  if (!shader) {
    ERROR("Failed to create shader of type: " << type);
  }
  glShaderSource(shader, 1, &shader_source, nullptr);
  glCompileShader(shader);
  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (!status) {
    GLint log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    char shader_log[log_length];
    glGetShaderInfoLog(shader, log_length, nullptr, shader_log);
    ERROR("Failed to compile shader: " << shader_log);
  }
  return shader;
}

GLuint CreateProgram() {
  GLuint vertex_shader = CompileShader(GL_VERTEX_SHADER, kVertexShaderPath);
  GLuint fragment_shader = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderPath);

  GLuint program = glCreateProgram();
  if (!program) {
    ERROR("Failed to create program");
  }
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  GLint status;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (!status) {
    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    char program_log[log_length];
    glGetProgramInfoLog(program, log_length, nullptr, program_log);
    ERROR("Failed to link program: " << program_log);
  }
  glDetachShader(program, vertex_shader);
  glDetachShader(program, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return program;
}

GLuint CreateVBO() {
  std::array<Vertex, 6> vertices = {};
  // Triangle 1 is upside-down and red.
  vertices[0] = { .position = {0.0, 0.5, 0.0}, .color = {1.0, 0.0, 0.0} };
  vertices[1] = { .position = {-0.5, -0.5, 0.0}, .color = {1.0, 0.0, 0.0} };
  vertices[2] = { .position = {0.5, -0.5, 0.0}, .color = {1.0, 0.0, 0.0} };
  // Triangle 1 is upright and green.
  vertices[3] = { .position = {0.0, -0.5, 0.0}, .color = {0.0, 1.0, 0.0} };
  vertices[4] = { .position = {0.5, 0.5, 0.0}, .color = {0.0, 1.0, 0.0} };
  vertices[5] = { .position = {-0.5, 0.5, 0.0}, .color = {0.0, 1.0, 0.0} };

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
  return vbo;
}

GLuint CreateVAO(GLuint vbo, const std::array<uint16_t, 3>& indices) {
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
      (void*)offsetof(Vertex, position));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
      (void*)offsetof(Vertex, color));
  glEnableVertexAttribArray(1);

  GLuint ebo;
  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_STATIC_DRAW);
  return vao;
}

void Draw(GLuint program, GLuint vao1, GLuint vao2, bool with_draw_arrays) {
  glViewport(0, 0, kWidth, kHeight);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(program);

  // "Initialize" the VAOs. 
  glBindVertexArray(vao1);
  glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);
  glBindVertexArray(vao2);
  glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);

  // Switch to VAO1 with an indexed draw to set the state's index buffer to
  // [0, 1, 2].
  glBindVertexArray(vao1);
  glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);

  // Switch to VAO2 and optionally perform a NON-indexed draw to set the
  // pipeline's vao address to VAO2.
  glBindVertexArray(vao2);
  if (with_draw_arrays) {
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }

  // Clear and perform an indexed draw.
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);
  glFinish();
}

GLubyte* ReadFramebuffer() {
  GLubyte* pixels = static_cast<GLubyte*>(malloc(kWidth * kHeight * 4));
  glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  return pixels;
}

void WriteImage(GLubyte* pixels, const std::string& filename) {
  FILE *f = fopen(filename.c_str(), "wb");

  png_image image_info = {};
  image_info.version = PNG_IMAGE_VERSION;
  image_info.width = kWidth;
  image_info.height = kHeight;
  image_info.format = PNG_FORMAT_RGBA;
  if (png_image_write_to_stdio(&image_info, f, 0, pixels, kWidth * 4, nullptr) == 0) {
    ERROR("Error writing PNG: " << image_info.message);
  }

  fclose(f);
}

int main() {
  InitializeEGL();
  SetUpFramebuffer();
  GLuint program = CreateProgram();
  GLuint vbo = CreateVBO();
  GLuint vao1 = CreateVAO(vbo, { 0, 1, 2 });
  GLuint vao2 = CreateVAO(vbo, { 3, 4, 5 });

  // Draw once with an extra glDrawArrays and once without. Since we glClear
  // right after calling glDrawArrays, both variants _should_ produce the same
  // image of the upright green triangle.
  {
    Draw(program, vao1, vao2, true /** with_draw_arrays */);
    GLubyte* pixels = ReadFramebuffer();
    WriteImage(pixels, "with_draw_arrays.png");
  }
  {
    Draw(program, vao1, vao2, false /** with_draw_arrays */);
    GLubyte* pixels = ReadFramebuffer();
    WriteImage(pixels, "without_draw_arrays.png");
  }

  eglMakeCurrent(display, EGL_NO_SURFACE,EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(display, context);
  eglTerminate(display);
  return EXIT_SUCCESS;
}
