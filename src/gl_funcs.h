#pragma once

// OpenGL function loading for cross-platform GPU benchmark.
//
// On Linux (dynamic build), GL functions are resolved at link time via
// GL_GLEXT_PROTOTYPES and linked from libGL.so.
//
// On Windows and Linux static builds (CB_NEED_GL_LOAD), we use ImGui GL3
// backend's built-in loader (imgl3w) as the base GL loader. This ensures ALL
// GL calls go through loaded function pointers via SDL_GL_GetProcAddress.
// On Windows this avoids Wine dispatch conflicts. On Linux static builds
// this eliminates the libGL.so link-time dependency (SDL2 dlopen's it).
//
// Functions not provided by imgl3w are loaded separately via SDL_GL_GetProcAddress.
// X-macro lists (CB_GL_*_FUNCS) are the single source of truth for which
// extra functions exist. Adding a new GL function = add typedef + add to list + add redirect.

#include <SDL.h>

#if defined(_WIN32) || defined(CB_STATIC_GL)
  #define CB_NEED_GL_LOAD

  // imgl3w provides: GL types, constants, ~50 GL functions (as macros),
  // and the imgl3wInit/imgl3wInit2 API.
  // On Windows it includes <windows.h> and defines APIENTRY.
  // On Linux (static build) APIENTRY is not defined — provide it.
  // Do NOT include <GL/gl.h> alongside this header.
  #ifndef _WIN32
    #ifndef APIENTRY
      #define APIENTRY
    #endif
  #endif
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

  // GPU timer query constants
  #ifndef GL_TIME_ELAPSED
  #define GL_TIME_ELAPSED                 0x88BF
  #endif
  #ifndef GL_QUERY_RESULT
  #define GL_QUERY_RESULT                 0x8866
  #endif
  #ifndef GL_QUERY_RESULT_AVAILABLE
  #define GL_QUERY_RESULT_AVAILABLE       0x8867
  #endif

  // FBO constants
  #ifndef GL_FRAMEBUFFER
  #define GL_FRAMEBUFFER                  0x8D40
  #endif
  #ifndef GL_COLOR_ATTACHMENT0
  #define GL_COLOR_ATTACHMENT0            0x8CE0
  #endif
  #ifndef GL_DEPTH_ATTACHMENT
  #define GL_DEPTH_ATTACHMENT             0x8D00
  #endif
  #ifndef GL_DEPTH_COMPONENT
  #define GL_DEPTH_COMPONENT              0x1902
  #endif
  #ifndef GL_DEPTH_COMPONENT24
  #define GL_DEPTH_COMPONENT24            0x81A6
  #endif
  #ifndef GL_FRAMEBUFFER_COMPLETE
  #define GL_FRAMEBUFFER_COMPLETE         0x8CD5
  #endif
  #ifndef GL_RENDERBUFFER
  #define GL_RENDERBUFFER                 0x8D41
  #endif
  #ifndef GL_READ_FRAMEBUFFER
  #define GL_READ_FRAMEBUFFER             0x8CA8
  #endif
  #ifndef GL_DRAW_FRAMEBUFFER
  #define GL_DRAW_FRAMEBUFFER             0x8CA9
  #endif
  #ifndef GL_UNSIGNED_INT
  #define GL_UNSIGNED_INT                 0x1405
  #endif

  // Blend constants
  #ifndef GL_FUNC_ADD
  #define GL_FUNC_ADD                     0x8006
  #endif
  #ifndef GL_BLEND_SRC_ALPHA
  #define GL_BLEND_SRC_ALPHA              0x80CB
  #endif
  #ifndef GL_BLEND_DST_ALPHA
  #define GL_BLEND_DST_ALPHA              0x80CA
  #endif

  // =========================================================================
  // Function pointer typedefs (unique signatures — must be defined manually)
  // =========================================================================

  // GL 1.x extras
  typedef void   (APIENTRY *PFNCB_glFinish)(void);
  typedef void   (APIENTRY *PFNCB_glCullFace)(GLenum);
  typedef void   (APIENTRY *PFNCB_glFrontFace)(GLenum);
  typedef void   (APIENTRY *PFNCB_glBlendFunc)(GLenum, GLenum);
  typedef void   (APIENTRY *PFNCB_glColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
  typedef void   (APIENTRY *PFNCB_glDepthMask)(GLboolean);
  typedef void   (APIENTRY *PFNCB_glDrawArrays)(GLenum, GLint, GLsizei);
  typedef void   (APIENTRY *PFNCB_glTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);

  // GL 2.0 extras
  typedef void   (APIENTRY *PFNCB_glUniform1f)(GLint, GLfloat);
  typedef void   (APIENTRY *PFNCB_glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
  typedef void   (APIENTRY *PFNCB_glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
  typedef void   (APIENTRY *PFNCB_glBindAttribLocation)(GLuint, GLuint, const GLchar*);

  // GL 3.0+ extras
  typedef void   (APIENTRY *PFNCB_glGenerateMipmap)(GLenum);
  typedef void   (APIENTRY *PFNCB_glDrawElementsInstanced)(GLenum, GLsizei, GLenum, const void*, GLsizei);
  typedef void   (APIENTRY *PFNCB_glVertexAttribDivisor)(GLuint, GLuint);

  // FBO
  typedef void   (APIENTRY *PFNCB_glGenFramebuffers)(GLsizei, GLuint*);
  typedef void   (APIENTRY *PFNCB_glDeleteFramebuffers)(GLsizei, const GLuint*);
  typedef void   (APIENTRY *PFNCB_glBindFramebuffer)(GLenum, GLuint);
  typedef void   (APIENTRY *PFNCB_glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
  typedef GLenum (APIENTRY *PFNCB_glCheckFramebufferStatus)(GLenum);
  typedef void   (APIENTRY *PFNCB_glGenRenderbuffers)(GLsizei, GLuint*);
  typedef void   (APIENTRY *PFNCB_glDeleteRenderbuffers)(GLsizei, const GLuint*);
  typedef void   (APIENTRY *PFNCB_glBindRenderbuffer)(GLenum, GLuint);
  typedef void   (APIENTRY *PFNCB_glRenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
  typedef void   (APIENTRY *PFNCB_glFramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
  typedef void   (APIENTRY *PFNCB_glBlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

  // =========================================================================
  // X-macro function lists — single source of truth.
  // Adding a new GL function:
  //   1. Add typedef above (unique signature)
  //   2. Add X(name) to the appropriate list below
  //   3. Add #define redirect in the section below
  // =========================================================================

  // Required GL 2.0 functions — fatal if missing
  #define CB_GL_REQUIRED_FUNCS(X) \
      X(glFinish) \
      X(glCullFace) \
      X(glFrontFace) \
      X(glBlendFunc) \
      X(glColorMask) \
      X(glDepthMask) \
      X(glDrawArrays) \
      X(glUniform1f) \
      X(glUniform3f) \
      X(glUniform4f) \
      X(glBindAttribLocation)

  // GL 2.0 soft-required — loaded but not fatal if missing
  #define CB_GL_SOFTREQ_FUNCS(X) \
      X(glTexSubImage2D)

  // GL 3.0+ optional functions (loaded in loadGL3Functions)
  #define CB_GL3_OPTIONAL_FUNCS(X) \
      X(glGenerateMipmap) \
      X(glDrawElementsInstanced) \
      X(glVertexAttribDivisor) \
      X(glGenFramebuffers) \
      X(glDeleteFramebuffers) \
      X(glBindFramebuffer) \
      X(glFramebufferTexture2D) \
      X(glCheckFramebufferStatus) \
      X(glGenRenderbuffers) \
      X(glDeleteRenderbuffers) \
      X(glBindRenderbuffer) \
      X(glRenderbufferStorage) \
      X(glFramebufferRenderbuffer) \
      X(glBlitFramebuffer)

  // =========================================================================
  // Extern declarations — generated from X-macro lists
  // =========================================================================
  #define CB_EXTERN_DECL(name) extern PFNCB_##name cb_##name;
  CB_GL_REQUIRED_FUNCS(CB_EXTERN_DECL)
  CB_GL_SOFTREQ_FUNCS(CB_EXTERN_DECL)
  CB_GL3_OPTIONAL_FUNCS(CB_EXTERN_DECL)
  #undef CB_EXTERN_DECL

  // =========================================================================
  // Redirect standard GL names to our pointers.
  // (Cannot be generated with X-macros — #define inside macro not allowed.)
  // Functions covered by imgl3w are already redirected via its macros.
  // =========================================================================

  // GL 1.x extras
  #define glFinish                        cb_glFinish
  #define glCullFace                      cb_glCullFace
  #define glFrontFace                     cb_glFrontFace
  #define glBlendFunc                     cb_glBlendFunc
  #define glColorMask                     cb_glColorMask
  #define glDepthMask                     cb_glDepthMask
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

  // FBO extras
  #define glGenFramebuffers               cb_glGenFramebuffers
  #define glDeleteFramebuffers            cb_glDeleteFramebuffers
  #define glBindFramebuffer               cb_glBindFramebuffer
  #define glFramebufferTexture2D          cb_glFramebufferTexture2D
  #define glCheckFramebufferStatus        cb_glCheckFramebufferStatus
  #define glGenRenderbuffers              cb_glGenRenderbuffers
  #define glDeleteRenderbuffers           cb_glDeleteRenderbuffers
  #define glBindRenderbuffer              cb_glBindRenderbuffer
  #define glRenderbufferStorage           cb_glRenderbufferStorage
  #define glFramebufferRenderbuffer       cb_glFramebufferRenderbuffer
  #define glBlitFramebuffer               cb_glBlitFramebuffer

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
