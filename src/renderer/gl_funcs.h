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

#if defined(_WIN32) || defined(__APPLE__) || defined(CB_STATIC_GL)
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

  // GL4 compute shader constants
  #ifndef GL_COMPUTE_SHADER
  #define GL_COMPUTE_SHADER               0x91B9
  #endif
  #ifndef GL_SHADER_STORAGE_BUFFER
  #define GL_SHADER_STORAGE_BUFFER        0x90D2
  #endif
  #ifndef GL_SHADER_STORAGE_BARRIER_BIT
  #define GL_SHADER_STORAGE_BARRIER_BIT   0x00002000
  #endif
  #ifndef GL_ALL_BARRIER_BITS
  #define GL_ALL_BARRIER_BITS             0xFFFFFFFF
  #endif

  // Texture format
  #ifndef GL_RED
  #define GL_RED                          0x1903
  #endif

  // Texture arrays (GL 3.0+)
  #ifndef GL_TEXTURE_2D_ARRAY
  #define GL_TEXTURE_2D_ARRAY             0x8C1A
  #endif

  // MRT additional color attachments
  #ifndef GL_COLOR_ATTACHMENT1
  #define GL_COLOR_ATTACHMENT1            0x8CE1
  #endif
  #ifndef GL_COLOR_ATTACHMENT2
  #define GL_COLOR_ATTACHMENT2            0x8CE2
  #endif
  #ifndef GL_COLOR_ATTACHMENT3
  #define GL_COLOR_ATTACHMENT3            0x8CE3
  #endif

  // Uniform buffer objects (GL 3.1+)
  #ifndef GL_UNIFORM_BUFFER
  #define GL_UNIFORM_BUFFER               0x8A11
  #endif

  // Rasterizer discard (GL 3.0+)
  #ifndef GL_RASTERIZER_DISCARD
  #define GL_RASTERIZER_DISCARD           0x8C89
  #endif

  // Transform feedback (GL 3.0+)
  #ifndef GL_TRANSFORM_FEEDBACK_BUFFER
  #define GL_TRANSFORM_FEEDBACK_BUFFER    0x8C8E
  #endif
  #ifndef GL_INTERLEAVED_ATTRIBS
  #define GL_INTERLEAVED_ATTRIBS          0x8C8C
  #endif
  #ifndef GL_POINTS
  #define GL_POINTS                       0x0000
  #endif

  // Indirect draw (GL 4.3+)
  #ifndef GL_DRAW_INDIRECT_BUFFER
  #define GL_DRAW_INDIRECT_BUFFER         0x8F3F
  #endif

  // Geometry shader (GL 3.2+)
  #ifndef GL_GEOMETRY_SHADER
  #define GL_GEOMETRY_SHADER              0x8DD9
  #endif

  // Tessellation (GL 4.0+)
  #ifndef GL_PATCHES
  #define GL_PATCHES                      0x000E
  #endif
  #ifndef GL_PATCH_VERTICES
  #define GL_PATCH_VERTICES               0x8E72
  #endif
  #ifndef GL_TESS_CONTROL_SHADER
  #define GL_TESS_CONTROL_SHADER          0x8E88
  #endif
  #ifndef GL_TESS_EVALUATION_SHADER
  #define GL_TESS_EVALUATION_SHADER       0x8E87
  #endif

  // Image load/store (GL 4.2+)
  #ifndef GL_READ_ONLY
  #define GL_READ_ONLY                    0x88B8
  #endif
  #ifndef GL_WRITE_ONLY
  #define GL_WRITE_ONLY                   0x88B9
  #endif
  #ifndef GL_READ_WRITE
  #define GL_READ_WRITE                   0x88BA
  #endif
  #ifndef GL_RGBA32F
  #define GL_RGBA32F                      0x8814
  #endif
  #ifndef GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
  #define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
  #endif

  // Buffer storage / persistent mapping (GL 4.4+)
  #ifndef GL_MAP_READ_BIT
  #define GL_MAP_READ_BIT                 0x0001
  #endif
  #ifndef GL_MAP_WRITE_BIT
  #define GL_MAP_WRITE_BIT                0x0002
  #endif
  #ifndef GL_MAP_PERSISTENT_BIT
  #define GL_MAP_PERSISTENT_BIT           0x0040
  #endif
  #ifndef GL_MAP_COHERENT_BIT
  #define GL_MAP_COHERENT_BIT             0x0080
  #endif
  #ifndef GL_DYNAMIC_STORAGE_BIT
  #define GL_DYNAMIC_STORAGE_BIT          0x0100
  #endif

  // Sync objects (GL 3.2+, needed for persistent mapping)
  #ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
  #define GL_SYNC_GPU_COMMANDS_COMPLETE   0x9117
  #endif
  #ifndef GL_ALREADY_SIGNALED
  #define GL_ALREADY_SIGNALED             0x911A
  #endif
  #ifndef GL_CONDITION_SATISFIED
  #define GL_CONDITION_SATISFIED          0x911C
  #endif
  #ifndef GL_TIMEOUT_EXPIRED
  #define GL_TIMEOUT_EXPIRED              0x911B
  #endif
  #ifndef GL_SYNC_FLUSH_COMMANDS_BIT
  #define GL_SYNC_FLUSH_COMMANDS_BIT      0x00000001
  #endif

  // GLsync type (guard prevents double-define if GL headers already provide it)
  #ifndef __gl_GLsync_defined
  typedef struct __GLsync* GLsync;
  #define __gl_GLsync_defined
  #endif

  // GL constant validation
  static_assert(GL_COLOR_ATTACHMENT1 == GL_COLOR_ATTACHMENT0 + 1, "Sequential");
  static_assert(GL_COLOR_ATTACHMENT2 == GL_COLOR_ATTACHMENT0 + 2, "Sequential");
  static_assert(GL_COLOR_ATTACHMENT3 == GL_COLOR_ATTACHMENT0 + 3, "Sequential");
  static_assert(GL_TEXTURE_2D_ARRAY == 0x8C1A, "");
  static_assert(GL_UNIFORM_BUFFER == 0x8A11, "");
  static_assert(GL_TRANSFORM_FEEDBACK_BUFFER == 0x8C8E, "");
  static_assert(GL_RASTERIZER_DISCARD == 0x8C89, "");
  static_assert(GL_GEOMETRY_SHADER == 0x8DD9, "");
  static_assert(GL_INTERLEAVED_ATTRIBS == 0x8C8C, "");
  static_assert(GL_DRAW_INDIRECT_BUFFER == 0x8F3F, "");
  static_assert(GL_PATCHES == 0x000E, "");
  static_assert(GL_PATCH_VERTICES == 0x8E72, "");
  static_assert(GL_TESS_CONTROL_SHADER == 0x8E88, "");
  static_assert(GL_TESS_EVALUATION_SHADER == 0x8E87, "");
  static_assert(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT == 0x00000020, "");
  static_assert(GL_MAP_PERSISTENT_BIT == 0x0040, "");
  static_assert(GL_MAP_COHERENT_BIT == 0x0080, "");
  static_assert(GL_SYNC_GPU_COMMANDS_COMPLETE == 0x9117, "");

  // =========================================================================
  // Function pointer typedefs (unique signatures — must be defined manually)
  // =========================================================================

  // GL 1.x extras
  using PFNCB_glFinish         = void   (APIENTRY *)(void);
  using PFNCB_glCullFace       = void   (APIENTRY *)(GLenum);
  using PFNCB_glFrontFace      = void   (APIENTRY *)(GLenum);
  using PFNCB_glBlendFunc      = void   (APIENTRY *)(GLenum, GLenum);
  using PFNCB_glColorMask      = void   (APIENTRY *)(GLboolean, GLboolean, GLboolean, GLboolean);
  using PFNCB_glDepthMask      = void   (APIENTRY *)(GLboolean);
  using PFNCB_glDrawArrays     = void   (APIENTRY *)(GLenum, GLint, GLsizei);
  using PFNCB_glTexSubImage2D  = void   (APIENTRY *)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);

  // GL 2.0 extras
  using PFNCB_glUniform1f           = void   (APIENTRY *)(GLint, GLfloat);
  using PFNCB_glUniform3f           = void   (APIENTRY *)(GLint, GLfloat, GLfloat, GLfloat);
  using PFNCB_glUniform4f           = void   (APIENTRY *)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
  using PFNCB_glBindAttribLocation  = void   (APIENTRY *)(GLuint, GLuint, const GLchar*);

  // GL 3.0+ extras
  using PFNCB_glGenerateMipmap          = void   (APIENTRY *)(GLenum);
  using PFNCB_glDrawElementsInstanced   = void   (APIENTRY *)(GLenum, GLsizei, GLenum, const void*, GLsizei);
  using PFNCB_glVertexAttribDivisor     = void   (APIENTRY *)(GLuint, GLuint);

  // Texture arrays (GL 3.0+)
  using PFNCB_glTexImage3D              = void   (APIENTRY *)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
  using PFNCB_glTexSubImage3D           = void   (APIENTRY *)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*);

  // MRT (GL 3.0+)
  using PFNCB_glDrawBuffers             = void   (APIENTRY *)(GLsizei, const GLenum*);

  // UBO (GL 3.1+)
  using PFNCB_glUniformBlockBinding     = void   (APIENTRY *)(GLuint, GLuint, GLuint);
  using PFNCB_glGetUniformBlockIndex    = GLuint (APIENTRY *)(GLuint, const GLchar*);
  using PFNCB_glBindBufferRange         = void   (APIENTRY *)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr);

  // Transform feedback (GL 3.0+)
  using PFNCB_glBeginTransformFeedback  = void   (APIENTRY *)(GLenum);
  using PFNCB_glEndTransformFeedback    = void   (APIENTRY *)(void);
  using PFNCB_glTransformFeedbackVaryings = void (APIENTRY *)(GLuint, GLsizei, const GLchar* const*, GLenum);

  // Geometry shader (GL 3.2+)
  // (uses compileShader with GL_GEOMETRY_SHADER constant — no new function needed)

  // FBO
  using PFNCB_glGenFramebuffers         = void   (APIENTRY *)(GLsizei, GLuint*);
  using PFNCB_glDeleteFramebuffers      = void   (APIENTRY *)(GLsizei, const GLuint*);
  using PFNCB_glBindFramebuffer         = void   (APIENTRY *)(GLenum, GLuint);
  using PFNCB_glFramebufferTexture2D    = void   (APIENTRY *)(GLenum, GLenum, GLenum, GLuint, GLint);
  using PFNCB_glCheckFramebufferStatus  = GLenum (APIENTRY *)(GLenum);
  using PFNCB_glGenRenderbuffers        = void   (APIENTRY *)(GLsizei, GLuint*);
  using PFNCB_glDeleteRenderbuffers     = void   (APIENTRY *)(GLsizei, const GLuint*);
  using PFNCB_glBindRenderbuffer        = void   (APIENTRY *)(GLenum, GLuint);
  using PFNCB_glRenderbufferStorage     = void   (APIENTRY *)(GLenum, GLenum, GLsizei, GLsizei);
  using PFNCB_glFramebufferRenderbuffer = void   (APIENTRY *)(GLenum, GLenum, GLenum, GLuint);
  using PFNCB_glBlitFramebuffer         = void   (APIENTRY *)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

  // GL 3.0+ SSBO binding
  using PFNCB_glBindBufferBase          = void   (APIENTRY *)(GLenum, GLuint, GLuint);

  // GL 4.3+ compute
  using PFNCB_glDispatchCompute         = void   (APIENTRY *)(GLuint, GLuint, GLuint);
  using PFNCB_glMemoryBarrier           = void   (APIENTRY *)(GLbitfield);

  // GL 4.3+ indirect draw
  using PFNCB_glDrawElementsIndirect    = void   (APIENTRY *)(GLenum, GLenum, const void*);
  using PFNCB_glMultiDrawElementsIndirect = void (APIENTRY *)(GLenum, GLenum, const void*, GLsizei, GLsizei);

  // GL 4.0 tessellation
  using PFNCB_glPatchParameteri = void (APIENTRY *)(GLenum, GLint);

  // GL 4.2 image load/store
  using PFNCB_glBindImageTexture = void (APIENTRY *)(GLuint, GLuint, GLint,
                                                      GLboolean, GLint, GLenum, GLenum);

  // GL 4.4 buffer storage + persistent mapping
  using PFNCB_glBufferStorage   = void   (APIENTRY *)(GLenum, GLsizeiptr, const void*, GLbitfield);
  using PFNCB_glMapBufferRange  = void*  (APIENTRY *)(GLenum, GLintptr, GLsizeiptr, GLbitfield);

  // GL 3.2+ sync objects (needed for persistent mapping)
  using PFNCB_glFenceSync       = GLsync (APIENTRY *)(GLenum, GLbitfield);
  using PFNCB_glClientWaitSync  = GLenum (APIENTRY *)(GLsync, GLbitfield, GLuint64);
  using PFNCB_glDeleteSync      = void   (APIENTRY *)(GLsync);



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
      X(glBlitFramebuffer) \
      X(glBindBufferBase) \
      X(glTexImage3D) \
      X(glTexSubImage3D) \
      X(glDrawBuffers) \
      X(glUniformBlockBinding) \
      X(glGetUniformBlockIndex) \
      X(glBindBufferRange) \
      X(glBeginTransformFeedback) \
      X(glEndTransformFeedback) \
      X(glTransformFeedbackVaryings)

  // GL 4.3+ optional functions (loaded in loadGL4Functions)
  #define CB_GL4_OPTIONAL_FUNCS(X) \
      X(glDispatchCompute) \
      X(glMemoryBarrier) \
      X(glDrawElementsIndirect) \
      X(glMultiDrawElementsIndirect) \
      X(glPatchParameteri) \
      X(glBindImageTexture) \
      X(glBufferStorage) \
      X(glMapBufferRange) \
      X(glFenceSync) \
      X(glClientWaitSync) \
      X(glDeleteSync)

  // =========================================================================
  // Extern declarations — generated from X-macro lists
  // =========================================================================
  #define CB_EXTERN_DECL(name) extern PFNCB_##name cb_##name;
  CB_GL_REQUIRED_FUNCS(CB_EXTERN_DECL)
  CB_GL_SOFTREQ_FUNCS(CB_EXTERN_DECL)
  CB_GL3_OPTIONAL_FUNCS(CB_EXTERN_DECL)
  CB_GL4_OPTIONAL_FUNCS(CB_EXTERN_DECL)
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
  #define glBindBufferBase                cb_glBindBufferBase

  // GL 3.0+ texture array / MRT / UBO / TF extras
  #define glTexImage3D                    cb_glTexImage3D
  #define glTexSubImage3D                 cb_glTexSubImage3D
  #define glDrawBuffers                   cb_glDrawBuffers
  #define glUniformBlockBinding           cb_glUniformBlockBinding
  #define glGetUniformBlockIndex          cb_glGetUniformBlockIndex
  #define glBindBufferRange               cb_glBindBufferRange
  #define glBeginTransformFeedback        cb_glBeginTransformFeedback
  #define glEndTransformFeedback          cb_glEndTransformFeedback
  #define glTransformFeedbackVaryings     cb_glTransformFeedbackVaryings

  // GL 4.3+ compute extras
  #define glDispatchCompute               cb_glDispatchCompute
  #define glMemoryBarrier                 cb_glMemoryBarrier

  // GL 4.3+ indirect draw extras
  #define glDrawElementsIndirect          cb_glDrawElementsIndirect
  #define glMultiDrawElementsIndirect     cb_glMultiDrawElementsIndirect

  // GL 4.0 tessellation
  #define glPatchParameteri               cb_glPatchParameteri

  // GL 4.2 image load/store
  #define glBindImageTexture              cb_glBindImageTexture

  // GL 4.4 buffer storage + persistent mapping
  #define glBufferStorage                 cb_glBufferStorage
  #define glMapBufferRange                cb_glMapBufferRange

  // GL 3.2+ sync objects
  #define glFenceSync                     cb_glFenceSync
  #define glClientWaitSync                cb_glClientWaitSync
  #define glDeleteSync                    cb_glDeleteSync


#else
  // Linux: all GL functions resolved at link time
  #define GL_GLEXT_PROTOTYPES
  #include <GL/gl.h>
  #include <GL/glext.h>
#endif // CB_NEED_GL_LOAD

// =========================================================================
// Extension-only functions — never core, always loaded via function pointers
// on ALL platforms (not available as link-time symbols on most systems).
// =========================================================================

// GLsync type (needed on both paths for persistent mapping)
#ifndef __gl_GLsync_defined
typedef struct __GLsync* GLsync;
#define __gl_GLsync_defined
#endif

#ifndef APIENTRY
  #define APIENTRY
#endif

// Typedefs for extension-only functions
using PFNCB_glGetTextureHandleARB             = GLuint64 (APIENTRY *)(GLuint);
using PFNCB_glMakeTextureHandleResidentARB    = void     (APIENTRY *)(GLuint64);
using PFNCB_glMakeTextureHandleNonResidentARB = void     (APIENTRY *)(GLuint64);
using PFNCB_glUniformHandleui64ARB            = void     (APIENTRY *)(GLint, GLuint64);

// X-macro list
#define CB_GL_EXT_FUNCS(X) \
    X(glGetTextureHandleARB) \
    X(glMakeTextureHandleResidentARB) \
    X(glMakeTextureHandleNonResidentARB) \
    X(glUniformHandleui64ARB)

// Extern declarations
#define CB_EXTERN_DECL(name) extern PFNCB_##name cb_##name;
CB_GL_EXT_FUNCS(CB_EXTERN_DECL)
#undef CB_EXTERN_DECL

// Redirects (always go through function pointers)
#define glGetTextureHandleARB             cb_glGetTextureHandleARB
#define glMakeTextureHandleResidentARB    cb_glMakeTextureHandleResidentARB
#define glMakeTextureHandleNonResidentARB cb_glMakeTextureHandleNonResidentARB
#define glUniformHandleui64ARB            cb_glUniformHandleui64ARB

// Load required GL functions. On Windows, initializes imgl3w and loads extras.
// Returns true on success.
bool loadGL2Functions();

// Load optional GL 3.0+ function pointers.
bool loadGL3Functions();

// Load optional GL 4.3+ function pointers.
bool loadGL4Functions();

// Load extension-only function pointers (ARB_bindless_texture etc).
bool loadGLExtFunctions();

// Low-level availability checks (used by GLLoader).
// On CB_NEED_GL_LOAD: check actual function pointers.
// On Linux: check GL version / extensions.
bool hasGL3FunctionPointers();
bool hasGL4FunctionPointers();
bool checkGL3ByVersion();
bool checkGL4ByVersion();

// Convenience: calls loadGL2Functions().
bool loadGLFunctions();
