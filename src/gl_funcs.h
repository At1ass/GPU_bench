#pragma once

// OpenGL function loading for cross-platform GPU benchmark.
//
// On Linux, GL functions are resolved at link time via GL_GLEXT_PROTOTYPES.
//
// On Windows (CB_NEED_GL_LOAD), we use ImGui GL3 backend's built-in loader
// (imgl3w) as the base GL loader. This ensures ALL GL calls — including GL 1.x
// functions like glClear, glEnable, glDrawElements — go through the same
// dispatch path (loaded function pointers via wglGetProcAddress with
// GetProcAddress(opengl32.dll) fallback). Under Wine, mixing link-time and
// loaded GL calls corrupts internal state and breaks rendering.
//
// Functions not provided by imgl3w are loaded separately via SDL_GL_GetProcAddress.

#include <SDL.h>

#ifdef _WIN32
  #define CB_NEED_GL_LOAD

  // imgl3w provides: GL types, constants, ~50 GL functions (as macros),
  // and the imgl3wInit/imgl3wInit2 API.
  // It includes <windows.h> and defines APIENTRY.
  // Do NOT include <GL/gl.h> alongside this header.
  #include <imgui_impl_opengl3_loader.h>

  // --- Constants not in imgl3w that we need ---
  #ifndef GL_DEPTH_BUFFER_BIT
  #define GL_DEPTH_BUFFER_BIT             0x00000100
  #endif
  #ifndef GL_STENCIL_BUFFER_BIT
  #define GL_STENCIL_BUFFER_BIT           0x00000400
  #endif
  #ifndef GL_STATIC_DRAW
  #define GL_STATIC_DRAW                  0x88E4
  #endif
  #ifndef GL_DYNAMIC_DRAW
  #define GL_DYNAMIC_DRAW                 0x88E8
  #endif
  #ifndef GL_CLAMP_TO_EDGE
  #define GL_CLAMP_TO_EDGE                0x812F
  #endif
  #ifndef GL_GENERATE_MIPMAP
  #define GL_GENERATE_MIPMAP              0x8191
  #endif
  #ifndef GL_MULTISAMPLE
  #define GL_MULTISAMPLE                  0x809D
  #endif
  #ifndef GL_MAX_TEXTURE_SIZE
  #define GL_MAX_TEXTURE_SIZE             0x0D33
  #endif
  #ifndef GL_RGB
  #define GL_RGB                          0x1907
  #endif
  #ifndef GL_LUMINANCE
  #define GL_LUMINANCE                    0x1909
  #endif
  #ifndef GL_REPEAT
  #define GL_REPEAT                       0x2901
  #endif
  #ifndef GL_NEAREST
  #define GL_NEAREST                      0x2600
  #endif
  #ifndef GL_TEXTURE_WRAP_S
  #define GL_TEXTURE_WRAP_S               0x2802
  #endif
  #ifndef GL_TEXTURE_WRAP_T
  #define GL_TEXTURE_WRAP_T               0x2803
  #endif
  #ifndef GL_LINEAR_MIPMAP_LINEAR
  #define GL_LINEAR_MIPMAP_LINEAR         0x2703
  #endif
  #ifndef GL_NO_ERROR
  #define GL_NO_ERROR                     0
  #endif
  #ifndef GL_CCW
  #define GL_CCW                          0x0901
  #endif

  // --- Extra GL types (imgl3w uses khronos types but we also need these) ---
  #ifndef GL_FUNC_ADD
  #define GL_FUNC_ADD                     0x8006
  #endif
  #ifndef GL_BLEND_SRC_ALPHA
  #define GL_BLEND_SRC_ALPHA              0x80CB
  #endif
  #ifndef GL_BLEND_DST_ALPHA
  #define GL_BLEND_DST_ALPHA              0x80CA
  #endif

  // GLsizeiptr / GLintptr — imgl3w provides these via khronos types,
  // but typedef them for compatibility if not already present.
  // imgl3w's khronos_ssize_t is the right size.

  // --- Function pointer types for GL functions NOT in imgl3w ---

  // GL 1.x functions not in imgl3w
  typedef void   (APIENTRY *PFNCB_glFinish)(void);
  typedef void   (APIENTRY *PFNCB_glCullFace)(GLenum);
  typedef void   (APIENTRY *PFNCB_glFrontFace)(GLenum);
  typedef void   (APIENTRY *PFNCB_glBlendFunc)(GLenum, GLenum);
  typedef void   (APIENTRY *PFNCB_glColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
  typedef void   (APIENTRY *PFNCB_glDrawArrays)(GLenum, GLint, GLsizei);
  typedef void   (APIENTRY *PFNCB_glTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);

  // GL 2.0 functions not in imgl3w
  typedef void   (APIENTRY *PFNCB_glUniform1f)(GLint, GLfloat);
  typedef void   (APIENTRY *PFNCB_glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
  typedef void   (APIENTRY *PFNCB_glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
  typedef void   (APIENTRY *PFNCB_glBindAttribLocation)(GLuint, GLuint, const GLchar*);

  // GL 3.0+ functions not in imgl3w
  typedef void   (APIENTRY *PFNCB_glGenerateMipmap)(GLenum);
  typedef void   (APIENTRY *PFNCB_glDrawElementsInstanced)(GLenum, GLsizei, GLenum, const void*, GLsizei);
  typedef void   (APIENTRY *PFNCB_glVertexAttribDivisor)(GLuint, GLuint);

  // --- Extern declarations for extra functions ---

  // GL 1.x extras
  extern PFNCB_glFinish                  cb_glFinish;
  extern PFNCB_glCullFace                cb_glCullFace;
  extern PFNCB_glFrontFace               cb_glFrontFace;
  extern PFNCB_glBlendFunc               cb_glBlendFunc;
  extern PFNCB_glColorMask               cb_glColorMask;
  extern PFNCB_glDrawArrays              cb_glDrawArrays;
  extern PFNCB_glTexSubImage2D           cb_glTexSubImage2D;

  // GL 2.0 extras
  extern PFNCB_glUniform1f               cb_glUniform1f;
  extern PFNCB_glUniform3f               cb_glUniform3f;
  extern PFNCB_glUniform4f               cb_glUniform4f;
  extern PFNCB_glBindAttribLocation      cb_glBindAttribLocation;

  // GL 3.0+ extras (optional)
  extern PFNCB_glGenerateMipmap          cb_glGenerateMipmap;
  extern PFNCB_glDrawElementsInstanced   cb_glDrawElementsInstanced;
  extern PFNCB_glVertexAttribDivisor     cb_glVertexAttribDivisor;

  // --- Redirect standard names to our pointers (extras only) ---
  // Functions covered by imgl3w are already redirected via its macros.

  // GL 1.x extras
  #define glFinish                        cb_glFinish
  #define glCullFace                      cb_glCullFace
  #define glFrontFace                     cb_glFrontFace
  #define glBlendFunc                     cb_glBlendFunc
  #define glColorMask                     cb_glColorMask
  #define glDrawArrays                    cb_glDrawArrays
  #define glTexSubImage2D                 cb_glTexSubImage2D

  // GL 2.0 extras
  #define glUniform1f                     cb_glUniform1f
  #define glUniform3f                     cb_glUniform3f
  #define glUniform4f                     cb_glUniform4f
  #define glBindAttribLocation            cb_glBindAttribLocation

  // GL 3.0+ extras
  #define glGenerateMipmap                cb_glGenerateMipmap
  #define glDrawElementsInstanced         cb_glDrawElementsInstanced
  #define glVertexAttribDivisor           cb_glVertexAttribDivisor

#else
  // Linux: all GL functions resolved at link time
  #define GL_GLEXT_PROTOTYPES
  #include <GL/gl.h>
  #include <GL/glext.h>
#endif // CB_NEED_GL_LOAD

// Load required GL functions. On Windows, initializes imgl3w and loads extras.
// Returns true on success.
bool loadGL2Functions();

// Load optional GL 3.0+ functions. Returns true if all available.
bool loadGL3Functions();

// Convenience: calls loadGL2Functions().
bool loadGLFunctions();
