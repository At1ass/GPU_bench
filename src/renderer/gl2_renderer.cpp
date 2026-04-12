#include <cstddef>
#include "renderer/gl2_renderer.h"
#include "renderer/gl_extensions.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>
#include <utility>

#ifdef __linux__
#include <SDL_syswm.h>
#include <dirent.h>
#endif

// ---- Embedded GLSL 1.20 shaders (compatibility profile) ----

static const char* VS_3D_120 = R"(
#version 120
attribute vec3 a_pos;
attribute vec3 a_normal;
attribute vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
varying vec3 v_normal;
varying vec2 v_uv;
void main() {
    v_normal = mat3(u_model) * a_normal;
    v_uv = a_uv;
    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);
}
)";

static const char* FS_3D_120 = R"(
#version 120
varying vec3 v_normal;
varying vec2 v_uv;
uniform sampler2D u_tex;
uniform float u_use_tex;
uniform vec4 u_color;
uniform vec3 u_light_dir;
void main() {
    vec3 n = normalize(v_normal);
    float d = max(dot(n, normalize(u_light_dir)), 0.0);
    float light = 0.15 + 0.85 * d;
    vec4 c = u_color;
    if (u_use_tex > 0.5)
        c = texture2D(u_tex, v_uv);
    gl_FragColor = vec4(c.rgb * light, c.a);
}
)";

static const char* VS_2D_120 = R"(
#version 120
attribute vec2 a_pos;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char* FS_2D_COLOR_120 = R"(
#version 120
uniform vec4 u_color;
void main() {
    gl_FragColor = u_color;
}
)";

static const char* VS_2D_TEX_120 = R"(
#version 120
attribute vec2 a_pos;
attribute vec2 a_uv;
varying vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char* FS_2D_TEX_120 = R"(
#version 120
varying vec2 v_uv;
uniform sampler2D u_tex;
void main() {
    gl_FragColor = texture2D(u_tex, v_uv);
}
)";

// ---- Embedded GLSL 1.50 shaders (core profile, macOS) ----

static const char* VS_3D_150 = R"(
#version 150
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
out vec3 v_normal;
out vec2 v_uv;
void main() {
    v_normal = mat3(u_model) * a_normal;
    v_uv = a_uv;
    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);
}
)";

static const char* FS_3D_150 = R"(
#version 150
in vec3 v_normal;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform float u_use_tex;
uniform vec4 u_color;
uniform vec3 u_light_dir;
out vec4 fragColor;
void main() {
    vec3 n = normalize(v_normal);
    float d = max(dot(n, normalize(u_light_dir)), 0.0);
    float light = 0.15 + 0.85 * d;
    vec4 c = u_color;
    if (u_use_tex > 0.5)
        c = texture(u_tex, v_uv);
    fragColor = vec4(c.rgb * light, c.a);
}
)";

static const char* VS_2D_150 = R"(
#version 150
in vec2 a_pos;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char* FS_2D_COLOR_150 = R"(
#version 150
uniform vec4 u_color;
out vec4 fragColor;
void main() {
    fragColor = u_color;
}
)";

static const char* VS_2D_TEX_150 = R"(
#version 150
in vec2 a_pos;
in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char* FS_2D_TEX_150 = R"(
#version 150
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 fragColor;
void main() {
    fragColor = texture(u_tex, v_uv);
}
)";

// ---- Implementation ----

GL2Renderer::GL2Renderer() : current_shader_(nullptr), initialized_(false),
    core_profile_(false), last_drawn_mesh_(INVALID_MESH),
    viewport_x_(0), viewport_y_(0), viewport_w_(0), viewport_h_(0),
    blit_quad_(INVALID_MESH), blit_quad_ready_(false),
    has_blit_framebuffer_(false) {
    // ShaderProg fields have default member initializers (program=0, uniforms=-1)
}

// ~GL2Renderer: default. shutdown() must be called explicitly before destruction.

GLuint GL2Renderer::compileShader(GLenum type, const char* src) {
    auto s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, static_cast<GLsizei>(sizeof(log)), nullptr, log);
        LOG_ERR("Shader compile error: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint GL2Renderer::linkProgram(GLuint vs, GLuint fs) {
    auto p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);

    // Force attribute locations so VAOs can use fixed layout
    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 1, "a_normal");
    glBindAttribLocation(p, 2, "a_uv");

    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, static_cast<GLsizei>(sizeof(log)), nullptr, log);
        LOG_ERR("Program link error: %s", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

bool GL2Renderer::buildShader(ShaderProg& prog, const char* vs_src, const char* fs_src) {
    auto vs = compileShader(GL_VERTEX_SHADER, vs_src);
    auto fs = compileShader(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) return false;
    prog.program = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!prog.program) return false;
    // Cache uniform locations (returns -1 if not found, which is fine)
    prog.u_proj      = glGetUniformLocation(prog.program, "u_proj");
    prog.u_view      = glGetUniformLocation(prog.program, "u_view");
    prog.u_model     = glGetUniformLocation(prog.program, "u_model");
    prog.u_color     = glGetUniformLocation(prog.program, "u_color");
    prog.u_light_dir = glGetUniformLocation(prog.program, "u_light_dir");
    prog.u_tex       = glGetUniformLocation(prog.program, "u_tex");
    prog.u_use_tex   = glGetUniformLocation(prog.program, "u_use_tex");
    // Cache attribute locations
    prog.a_pos    = glGetAttribLocation(prog.program, "a_pos");
    prog.a_normal = glGetAttribLocation(prog.program, "a_normal");
    prog.a_uv     = glGetAttribLocation(prog.program, "a_uv");
    return true;
}

void GL2Renderer::detectCaps() {
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &caps_.max_texture_size);

    // Query max vertex attributes
    #ifndef GL_MAX_VERTEX_ATTRIBS
    #define GL_MAX_VERTEX_ATTRIBS 0x8869u
    #endif
    caps_.max_vertex_attribs = 8;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &caps_.max_vertex_attribs);
    if (caps_.max_vertex_attribs < 8) caps_.max_vertex_attribs = 8;

    // Desktop GL always supports GL_UNSIGNED_INT indices since GL 1.1.
    caps_.supports_32bit_indices = true;

    // Parse GL version
    // Desktop GL: "4.6.0 NVIDIA 590.48.01"
    // GLES: "OpenGL ES 3.2 NVIDIA 590.48.01"
    const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    caps_.gl_major = 2;
    caps_.gl_minor = 0;
    if (ver) {
        const char* num = ver;
        // Skip "OpenGL ES " prefix if present
        const char* es = strstr(ver, "ES ");
        if (es) num = es + 3;
        // Skip "ES-CM " prefix (GLES 1.x)
        const char* escm = strstr(ver, "ES-CM ");
        if (escm) num = escm + 6;
        int major = 0, minor = 0;
        if (sscanf(num, "%d.%d", &major, &minor) >= 2) {
            caps_.gl_major = major;
            caps_.gl_minor = minor;
        }
    }

    // Detect GL2-level capabilities via function pointers first, extension strings as fallback.
    caps_.has_generate_mipmap_func = false;
    caps_.has_timer_queries = false;
    caps_.has_fbo = false;

#ifdef CB_NEED_GL_LOAD
    caps_.has_generate_mipmap_func = (cb_glGenerateMipmap != 0);
    caps_.has_fbo = (cb_glGenFramebuffers != 0)
                 && (cb_glBindFramebuffer != 0)
                 && (cb_glFramebufferTexture2D != 0)
                 && (cb_glCheckFramebufferStatus != 0);
    // Timer queries: check via SDL_GL_GetProcAddress (gpu_timer loads these separately)
    caps_.has_timer_queries = (SDL_GL_GetProcAddress("glGenQueries") != 0)
                           && (SDL_GL_GetProcAddress("glBeginQuery") != 0);
#else
    // Linux with GL_GLEXT_PROTOTYPES: symbols always exist at link time,
    // but may not be functional. Use GL version + extension strings.
    if (caps_.gl_major >= 3) {
        caps_.has_generate_mipmap_func = true;
        caps_.has_fbo = true;
        if (caps_.gl_major > 3 || (caps_.gl_major == 3 && caps_.gl_minor >= 3))
            caps_.has_timer_queries = true;
    }
    if (!caps_.has_generate_mipmap_func && GLExtensions::has("GL_ARB_framebuffer_object"))
        caps_.has_generate_mipmap_func = true;
    if (!caps_.has_fbo && GLExtensions::has("GL_ARB_framebuffer_object"))
        caps_.has_fbo = true;
    if (!caps_.has_fbo && GLExtensions::has("GL_EXT_framebuffer_object"))
        caps_.has_fbo = true;
    if (!caps_.has_timer_queries && GLExtensions::has("GL_ARB_timer_query"))
        caps_.has_timer_queries = true;
#endif

    // Try to detect VRAM. Multiple fallback methods for different drivers.
    caps_.estimated_vram_mb = 0;
#ifndef __APPLE__
    #ifndef GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
    #define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX 0x9048u
    #endif
    #ifndef GL_TEXTURE_FREE_MEMORY_ATI
    #define GL_TEXTURE_FREE_MEMORY_ATI 0x87FCu
    #endif

    // Method 1: NVIDIA proprietary driver
    if (GLExtensions::has("GL_NVX_gpu_memory_info")) {
        GLint kb = 0;
        glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &kb);
        if (kb > 0) caps_.estimated_vram_mb = kb / 1024;
        LOG_DBG("VRAM method 1 (GL_NVX_gpu_memory_info): %d MB", caps_.estimated_vram_mb);
    }
    // Method 2: AMD proprietary / Mesa RadeonSI (sometimes)
    if (caps_.estimated_vram_mb == 0 && GLExtensions::has("GL_ATI_meminfo")) {
        GLint info[4] = {0};
        glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, info);
        if (info[0] > 0) caps_.estimated_vram_mb = info[0] / 1024;
        LOG_DBG("VRAM method 2 (GL_ATI_meminfo): %d MB", caps_.estimated_vram_mb);
    }

#if !defined(_WIN32)
    // Method 3: GLX_MESA_query_renderer (Mesa: Nouveau, RadeonSI, Intel, llvmpipe, Zink)
    // Works on Linux and FreeBSD (anywhere Mesa + GLX is available)
    if (caps_.estimated_vram_mb == 0) {
        #define GLX_RENDERER_VIDEO_MEMORY_MESA 0x8187
        typedef int (*PFNGLXQUERYRENDERERMESA)(int, unsigned int*);
        PFNGLXQUERYRENDERERMESA queryRenderer =
            reinterpret_cast<PFNGLXQUERYRENDERERMESA>(SDL_GL_GetProcAddress("glXQueryCurrentRendererIntegerMESA"));
        LOG_DBG("VRAM method 3 (GLX_MESA): func=%s", queryRenderer ? "found" : "NULL");
        if (queryRenderer) {
            unsigned int vram_mb = 0;
            if (queryRenderer(GLX_RENDERER_VIDEO_MEMORY_MESA, &vram_mb) && vram_mb > 0) {
                caps_.estimated_vram_mb = static_cast<int>(vram_mb);
            }
            LOG_DBG("VRAM method 3 (GLX_MESA): %d MB", caps_.estimated_vram_mb);
        }
    }
#endif // !_WIN32

#if defined(__linux__) && !defined(__ANDROID__)
    // Method 4: Linux sysfs — scan /sys/class/drm/card*/device/mem_info_vram_total (AMD)
    if (caps_.estimated_vram_mb == 0) {
        LOG_DBG("VRAM method 4 (sysfs): scanning /sys/class/drm/");
        DIR* drm_dir = opendir("/sys/class/drm");
        if (drm_dir) {
            struct dirent* entry;
            while ((entry = readdir(drm_dir)) != nullptr) {
                if (strncmp(entry->d_name, "card", 4) != 0) continue;
                // Skip render nodes like "card0-DP-1"
                if (strchr(entry->d_name, '-')) continue;

                char path[512];
                // AMD: mem_info_vram_total (bytes)
                snprintf(path, sizeof(path), "/sys/class/drm/%s/device/mem_info_vram_total", entry->d_name);
                FILE* f = fopen(path, "r");
                if (f) {
                    unsigned long long bytes = 0;
                    if (fscanf(f, "%llu", &bytes) == 1 && bytes > 0) {
                        caps_.estimated_vram_mb = static_cast<int>(bytes / (1024ULL * 1024ULL));
                        LOG_DBG("VRAM method 4: %s = %d MB", entry->d_name, caps_.estimated_vram_mb);
                    }
                    fclose(f);
                    if (caps_.estimated_vram_mb > 0) break;
                }
            }
            closedir(drm_dir);
        }
    }
#endif // __linux__ && !__ANDROID__
#endif // !__APPLE__

    LOG_INF("GL Caps: GL %d.%d, max_tex=%d, max_attribs=%d, vram=%dMB, "
            "fbo=%s, timer_q=%s",
            caps_.gl_major, caps_.gl_minor,
            caps_.max_texture_size, caps_.max_vertex_attribs,
            caps_.estimated_vram_mb,
            caps_.has_fbo ? "yes" : "no",
            caps_.has_timer_queries ? "yes" : "no");
}

bool GL2Renderer::init(int w, int h) {
    if (initialized_) return true;

    state_cache_.reset();

    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    gpu_vendor_   = vendor   ? vendor   : "Unknown";
    gpu_renderer_ = renderer ? renderer : "Unknown";
    gl_version_   = version  ? version  : "Unknown";
    LOG_DBG("GL2Renderer::init: vendor=%s renderer=%s version=%s",
             gpu_vendor_.c_str(), gpu_renderer_.c_str(), gl_version_.c_str());

    detectCaps();
    LOG_DBG("GL2Renderer::init: detectCaps done");

    // Init GPU timer if available
    gpu_timer_.init();

    // Check if glBlitFramebuffer is available
#ifdef CB_NEED_GL_LOAD
    has_blit_framebuffer_ = (cb_glBlitFramebuffer != 0);
#else
    has_blit_framebuffer_ = (caps_.gl_major >= 3);
#endif

    // Slot 0 for render targets is reserved as "invalid"
    render_targets_.emplace_back();

    // Detect core profile: GL 3.2+ core doesn't support GLSL 1.20/1.40
    if (caps_.gl_major > 3 || (caps_.gl_major == 3 && caps_.gl_minor >= 2)) {
        int profile = 0;
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
        core_profile_ = (profile == SDL_GL_CONTEXT_PROFILE_CORE);
    }

    LOG_DBG("GL2Renderer::init: core_profile=%s, building shaders (GLSL %s)",
             core_profile_ ? "yes" : "no", core_profile_ ? "1.50" : "1.20");
    if (core_profile_) {
        LOG_INF("Core profile detected, using GLSL 1.50 shaders");
        if (!buildShader(shader_3d_, VS_3D_150, FS_3D_150)) return false;
        if (!buildShader(shader_2d_color_, VS_2D_150, FS_2D_COLOR_150)) return false;
        if (!buildShader(shader_2d_tex_, VS_2D_TEX_150, FS_2D_TEX_150)) return false;
    } else {
        if (!buildShader(shader_3d_, VS_3D_120, FS_3D_120)) return false;
        if (!buildShader(shader_2d_color_, VS_2D_120, FS_2D_COLOR_120)) return false;
        if (!buildShader(shader_2d_tex_, VS_2D_TEX_120, FS_2D_TEX_120)) return false;
    }
    LOG_DBG("GL2Renderer::init: shaders built OK");

    // Slot 0 for all meshes/textures/custom_shaders is reserved as "invalid"
    meshes_.emplace_back();
    meshes_[0].valid = false;
    textures_.emplace_back();
    textures_[0].valid = false;
    custom_shaders_.emplace_back(0); // slot 0 = invalid

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Set sensible default uniforms for all shaders
    ShaderProg* shaders[] = {&shader_3d_, &shader_2d_color_, &shader_2d_tex_};
    for (auto* sp : shaders) {
        glUseProgram(sp->program);
        if (sp->u_color >= 0)     glUniform4f(sp->u_color, 1.0f, 1.0f, 1.0f, 1.0f);
        if (sp->u_light_dir >= 0) glUniform3f(sp->u_light_dir, 0.4f, 0.7f, 0.5f);
        if (sp->u_use_tex >= 0)   glUniform1f(sp->u_use_tex, 0.0f);
        if (sp->u_tex >= 0)       glUniform1i(sp->u_tex, 0);
    }
    glUseProgram(0);

    setViewport(0, 0, w, h);
    initialized_ = true;
    return true;
}

void GL2Renderer::shutdown() {
    if (!initialized_) return;

    // Clean up blit quad before meshes (uses destroyMesh)
    if (blit_quad_ready_) {
        destroyMesh(blit_quad_);
        blit_quad_ = INVALID_MESH;
        blit_quad_ready_ = false;
    }

    for (size_t i = 1; i < meshes_.size(); i++) {
        if (meshes_[i].valid) {
            glDeleteBuffers(1, &meshes_[i].vbo);
            glDeleteBuffers(1, &meshes_[i].ibo);
        }
    }
    meshes_.clear();
    free_mesh_slots_.clear();

    for (size_t i = 1; i < textures_.size(); i++) {
        if (textures_[i].valid)
            glDeleteTextures(1, &textures_[i].id);
    }
    textures_.clear();
    free_tex_slots_.clear();

    for (size_t i = 1; i < custom_shaders_.size(); i++) {
        if (custom_shaders_[i])
            glDeleteProgram(custom_shaders_[i]);
    }
    custom_shaders_.clear();
    free_custom_slots_.clear();

    // Clean up render targets (FBOs)
    for (size_t i = 1; i < render_targets_.size(); i++) {
        if (render_targets_[i].valid) {
            if (render_targets_[i].fbo) glDeleteFramebuffers(1, &render_targets_[i].fbo);
            if (render_targets_[i].color_tex) glDeleteTextures(1, &render_targets_[i].color_tex);
            if (render_targets_[i].depth_rb) glDeleteRenderbuffers(1, &render_targets_[i].depth_rb);
        }
    }
    render_targets_.clear();
    free_rt_slots_.clear();

    if (shader_3d_.program) { glDeleteProgram(shader_3d_.program); shader_3d_.program = 0; }
    if (shader_2d_color_.program) { glDeleteProgram(shader_2d_color_.program); shader_2d_color_.program = 0; }
    if (shader_2d_tex_.program) { glDeleteProgram(shader_2d_tex_.program); shader_2d_tex_.program = 0; }

    current_shader_ = nullptr;
    initialized_ = false;
}

void GL2Renderer::setViewport(int x, int y, int w, int h) {
    viewport_x_ = x;
    viewport_y_ = y;
    viewport_w_ = w;
    viewport_h_ = h;
    glViewport(x, y, w, h);
}

void GL2Renderer::clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

MeshHandle GL2Renderer::createMesh(const MeshData& data) {
    GLMesh gm;
    gm.valid = true;
    gm.vao = 0;
    gm.index_count = static_cast<int>(data.indices.size());

    glGenBuffers(1, &gm.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gm.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(Vertex)),
                 data.vertices.data(), GL_STATIC_DRAW);

    // Determine if 16-bit indices are sufficient
    bool needs_32bit = false;
    for (size_t i = 0; i < data.indices.size(); i++) {
        if (data.indices[i] > 65535) {
            needs_32bit = true;
            break;
        }
    }

    glGenBuffers(1, &gm.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm.ibo);

    if (needs_32bit && caps_.supports_32bit_indices) {
        gm.index_type = GL_UNSIGNED_INT;
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.indices.size() * sizeof(unsigned int)),
                     data.indices.data(), GL_STATIC_DRAW);
    } else if (needs_32bit) {
        LOG_ERR("ERROR: mesh requires 32-bit indices but hardware doesn't support them");
        glDeleteBuffers(1, &gm.vbo);
        glDeleteBuffers(1, &gm.ibo);
        return INVALID_MESH;
    } else {
        gm.index_type = GL_UNSIGNED_SHORT;
        std::vector<unsigned short> indices16(data.indices.size());
        for (size_t i = 0; i < data.indices.size(); i++)
            indices16[i] = static_cast<unsigned short>(data.indices[i]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices16.size() * sizeof(unsigned short)),
                     indices16.data(), GL_STATIC_DRAW);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    MeshHandle h;
    if (!free_mesh_slots_.empty()) {
        h = free_mesh_slots_.back();
        free_mesh_slots_.pop_back();
        meshes_[h.id] = std::move(gm);
    } else {
        h = MeshHandle(static_cast<unsigned int>(meshes_.size()));
        meshes_.push_back(std::move(gm));
    }
    LOG_DBG("GL2: createMesh %u verts, %u indices -> handle %u",
             (unsigned)data.vertices.size(), (unsigned)data.indices.size(), (unsigned)h);
    return h;
}

void GL2Renderer::destroyMesh(MeshHandle h) {
    if (!isValidMesh(h)) return;
    glDeleteBuffers(1, &meshes_[h.id].vbo);
    glDeleteBuffers(1, &meshes_[h.id].ibo);
    meshes_[h.id].valid = false;
    free_mesh_slots_.push_back(h);
}

TextureHandle GL2Renderer::createTexture(int w, int h, int channels, const unsigned char* pixels) {
    if (w > caps_.max_texture_size || h > caps_.max_texture_size) {
        int nw = w, nh = h;
        while (nw > caps_.max_texture_size) nw /= 2;
        while (nh > caps_.max_texture_size) nh /= 2;
        LOG_WRN("GL2: texture %dx%d exceeds max %d, clamping to %dx%d",
                w, h, caps_.max_texture_size, nw, nh);

        std::vector<unsigned char> scaled(static_cast<size_t>(nw) * static_cast<size_t>(nh) * static_cast<size_t>(channels));
        for (int sy = 0; sy < nh; sy++) {
            for (int sx = 0; sx < nw; sx++) {
                int src_x = sx * w / nw;
                int src_y = sy * h / nh;
                int dst_idx = (sy * nw + sx) * channels;
                int src_idx = (src_y * w + src_x) * channels;
                for (int c = 0; c < channels; c++)
                    scaled[static_cast<size_t>(dst_idx + c)] = pixels[static_cast<size_t>(src_idx + c)];
            }
        }
        return createTexture(nw, nh, channels, scaled.data());
    }

    GLTex gt;
    gt.valid = true;
    glGenTextures(1, &gt.id);
    glBindTexture(GL_TEXTURE_2D, gt.id);

    // Mipmap generation: use glGenerateMipmap on GL3+, legacy GL_GENERATE_MIPMAP on GL2
    if (!caps_.has_generate_mipmap_func) {
        #ifndef GL_GENERATE_MIPMAP
        #define GL_GENERATE_MIPMAP 0x8191u
        #endif
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // GL_LUMINANCE removed in core profile — use GL_RED instead
    GLenum fmt;
    if (channels == 4) fmt = GL_RGBA;
    else if (channels == 3) fmt = GL_RGB;
    else fmt = (caps_.gl_major >= 3) ? GL_RED : GL_LUMINANCE;
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt), w, h, 0, fmt, GL_UNSIGNED_BYTE, pixels);

    if (caps_.has_generate_mipmap_func) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    TextureHandle th;
    if (!free_tex_slots_.empty()) {
        th = free_tex_slots_.back();
        free_tex_slots_.pop_back();
        textures_[th.id] = std::move(gt);
    } else {
        th = TextureHandle(static_cast<unsigned int>(textures_.size()));
        textures_.push_back(std::move(gt));
    }
    LOG_DBG("GL2: createTexture %dx%d ch=%d -> handle %u", w, h, channels, (unsigned)th);
    return th;
}

void GL2Renderer::destroyTexture(TextureHandle h) {
    if (!isValidTexture(h)) return;
    if (!textures_[h.id].rt_owned)
        glDeleteTextures(1, &textures_[h.id].id);
    textures_[h.id].valid = false;
    textures_[h.id].rt_owned = false;
    free_tex_slots_.push_back(h);
}

void GL2Renderer::useShader(ShaderType type) {
    switch (type) {
        case ShaderType::Scene3D:     current_shader_ = &shader_3d_; break;
        case ShaderType::Color2D:    current_shader_ = &shader_2d_color_; break;
        case ShaderType::Textured2D: current_shader_ = &shader_2d_tex_; break;
    }
    last_drawn_mesh_ = INVALID_MESH; // shader change invalidates attrib state
    glUseProgram(current_shader_->program);
}

ShaderHandle GL2Renderer::createCustomShader(const char* vs_src, const char* fs_src) {
    auto vs = compileShader(GL_VERTEX_SHADER, vs_src);
    auto fs = compileShader(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return INVALID_SHADER;
    }
    auto prog = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!prog) return INVALID_SHADER;

    ShaderHandle h;
    if (!free_custom_slots_.empty()) {
        h = free_custom_slots_.back();
        free_custom_slots_.pop_back();
        custom_shaders_[h.id] = prog;
    } else {
        h = ShaderHandle(static_cast<unsigned int>(custom_shaders_.size()));
        custom_shaders_.push_back(prog);
    }
    return h;
}

void GL2Renderer::useCustomShader(ShaderHandle h) {
    if (!isValidShader(h)) return;
    current_shader_ = nullptr; // no built-in shader active
    last_drawn_mesh_ = INVALID_MESH; // shader change invalidates attrib state
    glUseProgram(custom_shaders_[h.id]);
    renderer_stats_.shader_switches++;
}

void GL2Renderer::destroyCustomShader(ShaderHandle h) {
    if (!isValidShader(h)) return;
    glDeleteProgram(custom_shaders_[h.id]);
    custom_shaders_[h.id] = 0;
    free_custom_slots_.push_back(h);
}

int GL2Renderer::getCustomUniformLoc(ShaderHandle h, const char* name) {
    if (!isValidShader(h)) return -1;
    return glGetUniformLocation(custom_shaders_[h.id], name);
}

void GL2Renderer::setUniform1i(int loc, int v) {
    if (loc >= 0) glUniform1i(loc, v);
}

void GL2Renderer::setUniform1f(int loc, float v) {
    if (loc >= 0) glUniform1f(loc, v);
}

void GL2Renderer::setUniform2f(int loc, float x, float y) {
    if (loc >= 0) {
        #ifdef CB_NEED_GL_LOAD
        // glUniform2f not in our X-macro list — use glUniform3f with z=0 as workaround
        // Actually let's just call it via SDL proc
        typedef void (APIENTRY *PFN_glUniform2f)(GLint, GLfloat, GLfloat);
        static PFN_glUniform2f fn = (PFN_glUniform2f)SDL_GL_GetProcAddress("glUniform2f");
        if (fn) fn(loc, x, y);
        #else
        glUniform2f(loc, x, y);
        #endif
    }
}

void GL2Renderer::setUniform3f(int loc, float x, float y, float z) {
    if (loc >= 0) glUniform3f(loc, x, y, z);
}

void GL2Renderer::setUniform4f(int loc, float r, float g, float b, float a) {
    if (loc >= 0) glUniform4f(loc, r, g, b, a);
}

void GL2Renderer::setUniformMat4(int loc, const Mat4& m) {
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, m.ptr());
}

void GL2Renderer::setProjection(const Mat4& m) {
    if (current_shader_ && current_shader_->u_proj >= 0)
        glUniformMatrix4fv(current_shader_->u_proj, 1, GL_FALSE, m.ptr());
}

void GL2Renderer::setView(const Mat4& m) {
    if (current_shader_ && current_shader_->u_view >= 0)
        glUniformMatrix4fv(current_shader_->u_view, 1, GL_FALSE, m.ptr());
}

void GL2Renderer::setModel(const Mat4& m) {
    if (current_shader_ && current_shader_->u_model >= 0)
        glUniformMatrix4fv(current_shader_->u_model, 1, GL_FALSE, m.ptr());
}

void GL2Renderer::setColor(float r, float g, float b, float a) {
    if (current_shader_ && current_shader_->u_color >= 0)
        glUniform4f(current_shader_->u_color, r, g, b, a);
}

void GL2Renderer::setLightDir(float x, float y, float z) {
    if (current_shader_ && current_shader_->u_light_dir >= 0)
        glUniform3f(current_shader_->u_light_dir, x, y, z);
}

void GL2Renderer::bindTexture(TextureHandle h) {
    if (!isValidTexture(h)) {
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_[h.id].id);
    if (current_shader_ && current_shader_->u_tex >= 0)
        glUniform1i(current_shader_->u_tex, 0);
}

void GL2Renderer::bindTextureUnit(int unit, TextureHandle h) {
    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
    if (isValidTexture(h))
        glBindTexture(GL_TEXTURE_2D, textures_[h.id].id);
    else
        glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    renderer_stats_.texture_binds++;
}

void GL2Renderer::setUseTexture(bool use) {
    if (current_shader_ && current_shader_->u_use_tex >= 0)
        glUniform1f(current_shader_->u_use_tex, use ? 1.0f : 0.0f);
}

void GL2Renderer::drawMesh(MeshHandle h) {
    if (!isValidMesh(h)) return;
    const GLMesh& gm = meshes_[h.id];

    // Skip redundant vertex attrib setup when drawing the same mesh repeatedly
    if (h != last_drawn_mesh_) {
        glBindBuffer(GL_ARRAY_BUFFER, gm.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm.ibo);

        GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));

        // For custom shaders, use fixed attribute locations 0, 1, 2
        GLint loc_pos = 0, loc_normal = 1, loc_uv = 2;
        if (current_shader_) {
            loc_pos    = current_shader_->a_pos;
            loc_normal = current_shader_->a_normal;
            loc_uv     = current_shader_->a_uv;
        }

        if (loc_pos >= 0) {
            glEnableVertexAttribArray(static_cast<GLuint>(loc_pos));
            glVertexAttribPointer(static_cast<GLuint>(loc_pos), 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        }
        if (loc_normal >= 0) {
            glEnableVertexAttribArray(static_cast<GLuint>(loc_normal));
            glVertexAttribPointer(static_cast<GLuint>(loc_normal), 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, normal)));
        }
        if (loc_uv >= 0) {
            glEnableVertexAttribArray(static_cast<GLuint>(loc_uv));
            glVertexAttribPointer(static_cast<GLuint>(loc_uv), 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, uv)));
        }

        last_drawn_mesh_ = h;
    }

    glDrawElements(GL_TRIANGLES, gm.index_count, gm.index_type, 0);
    renderer_stats_.draw_calls++;
}

void GL2Renderer::uploadTextureData(TextureHandle h, int width, int height, int channels, const unsigned char* pixels) {
    if (!isValidTexture(h)) return;
    glBindTexture(GL_TEXTURE_2D, textures_[h.id].id);
    GLenum fmt = (channels == 4) ? GL_RGBA : (channels == 3) ? GL_RGB : GL_LUMINANCE;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, fmt, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GL2Renderer::setColorMask(bool r, bool g, bool b, bool a) {
    if (!state_cache_.setColorMask(r, g, b, a)) { renderer_stats_.state_skipped++; return; }
    renderer_stats_.state_applied++;
    glColorMask(r ? GL_TRUE : GL_FALSE, g ? GL_TRUE : GL_FALSE,
                b ? GL_TRUE : GL_FALSE, a ? GL_TRUE : GL_FALSE);
}

void GL2Renderer::setBlending(bool enable) {
    if (!state_cache_.setBlending(enable)) { renderer_stats_.state_skipped++; return; }
    renderer_stats_.state_applied++;
    if (enable) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

void GL2Renderer::setBlendingAdditive(bool enable) {
    if (!state_cache_.setBlendingAdditive(enable)) { renderer_stats_.state_skipped++; return; }
    renderer_stats_.state_applied++;
    if (enable) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
    } else {
        glDisable(GL_BLEND);
    }
}

void GL2Renderer::setDepthTest(bool enable) {
    if (!state_cache_.setDepthTest(enable)) { renderer_stats_.state_skipped++; return; }
    renderer_stats_.state_applied++;
    if (enable)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
}

void GL2Renderer::setCullFace(bool enable) {
    if (!state_cache_.setCullFace(enable)) { renderer_stats_.state_skipped++; return; }
    renderer_stats_.state_applied++;
    if (enable)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
}

void GL2Renderer::setDepthMask(bool write) {
    if (!state_cache_.setDepthMask(write)) { renderer_stats_.state_skipped++; return; }
    renderer_stats_.state_applied++;
    glDepthMask(write ? GL_TRUE : GL_FALSE);
}

void GL2Renderer::setPolygonOffset(bool enable, float factor, float units) {
    renderer_stats_.state_applied++;
    if (enable) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(factor, units);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

void GL2Renderer::resetState() {
    state_cache_.reset();
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    current_shader_ = nullptr;
    last_drawn_mesh_ = INVALID_MESH;
    glViewport(viewport_x_, viewport_y_, viewport_w_, viewport_h_);
}

// --- Unbind state (for ImGui interop) ---
void GL2Renderer::unbindState() {
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    current_shader_ = nullptr;
    last_drawn_mesh_ = INVALID_MESH;
}

// --- Scissor ---
void GL2Renderer::setScissor(bool enable, int x, int y, int w, int h) {
    if (enable) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, w, h);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

// --- Sync ---
void GL2Renderer::finish() {
    glFinish();
}

// --- Pixel readback ---
void GL2Renderer::readPixels(int x, int y, int w, int h, unsigned char* rgba_out) {
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba_out);
}

void GL2Renderer::copyFramebufferToTexture(TextureHandle tex, int w, int h) {
    if (!isValidTexture(tex)) return;
    glBindTexture(GL_TEXTURE_2D, textures_[tex.id].id);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// --- Render targets ---
bool GL2Renderer::supportsRenderTargets() const {
    return caps_.has_fbo;
}

RenderTargetHandle GL2Renderer::createRenderTarget(int w, int h) {
    if (!caps_.has_fbo) return INVALID_RENDER_TARGET;

    GLFBO rt;
    rt.w = w;
    rt.h = h;

    glGenTextures(1, &rt.color_tex);
    glBindTexture(GL_TEXTURE_2D, rt.color_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA), w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &rt.depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rt.depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &rt.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.color_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt.depth_rb);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &rt.fbo);
        glDeleteTextures(1, &rt.color_tex);
        glDeleteRenderbuffers(1, &rt.depth_rb);
        return INVALID_RENDER_TARGET;
    }

    rt.valid = true;

    RenderTargetHandle handle;
    if (!free_rt_slots_.empty()) {
        handle = free_rt_slots_.back();
        free_rt_slots_.pop_back();
        render_targets_[handle.id] = std::move(rt);
    } else {
        handle = RenderTargetHandle(static_cast<unsigned int>(render_targets_.size()));
        render_targets_.push_back(std::move(rt));
    }
    LOG_DBG("GL2: createRenderTarget %dx%d -> handle %u", w, h, (unsigned)handle);
    return handle;
}

void GL2Renderer::destroyRenderTarget(RenderTargetHandle rt) {
    if (!isValidRenderTarget(rt)) return;
    LOG_DBG("GL2: destroyRenderTarget handle %u", (unsigned)rt);
    GLFBO& fbo = render_targets_[rt.id];
    if (fbo.fbo)       glDeleteFramebuffers(1, &fbo.fbo);
    if (fbo.color_tex) glDeleteTextures(1, &fbo.color_tex);
    for (int i = 0; i < fbo.num_extra_color; i++)
        if (fbo.extra_color_tex[i]) glDeleteTextures(1, &fbo.extra_color_tex[i]);
    if (fbo.depth_rb)  glDeleteRenderbuffers(1, &fbo.depth_rb);
    if (fbo.depth_tex) glDeleteTextures(1, &fbo.depth_tex);
    fbo = GLFBO();
    free_rt_slots_.push_back(rt);
}

void GL2Renderer::bindRenderTarget(RenderTargetHandle rt) {
    renderer_stats_.rt_switches++;
    if (rt == INVALID_RENDER_TARGET) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    if (rt.id < render_targets_.size() && render_targets_[rt.id].valid) {
        glBindFramebuffer(GL_FRAMEBUFFER, render_targets_[rt.id].fbo);
    }
}

void GL2Renderer::bindRenderTargetTexture(RenderTargetHandle rt, int unit) {
    if (!isValidRenderTarget(rt)) return;
    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
    glBindTexture(GL_TEXTURE_2D, render_targets_[rt.id].color_tex);
    glActiveTexture(GL_TEXTURE0);
}

void GL2Renderer::blitToScreen(RenderTargetHandle rt,
                                int dst_x, int dst_y, int dst_w, int dst_h) {
    if (!isValidRenderTarget(rt)) return;
    const GLFBO& fbo = render_targets_[rt.id];

    if (has_blit_framebuffer_) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(
            0, 0, fbo.w, fbo.h,
            dst_x, dst_y, dst_x + dst_w, dst_y + dst_h,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
        // GL2 fallback: textured quad
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (!blit_quad_ready_) {
            MeshData qd;
            qd.vertices = {
                {{-1, -1, 0}, {0, 0, 1}, {0, 0}},
                {{ 1, -1, 0}, {0, 0, 1}, {1, 0}},
                {{ 1,  1, 0}, {0, 0, 1}, {1, 1}},
                {{-1,  1, 0}, {0, 0, 1}, {0, 1}},
            };
            qd.indices = {0, 1, 2, 0, 2, 3};
            blit_quad_ = createMesh(qd);
            blit_quad_ready_ = true;
        }
        setViewport(dst_x, dst_y, dst_w, dst_h);
        setDepthTest(false);
        setBlending(false);
        useShader(ShaderType::Textured2D);
        glBindTexture(GL_TEXTURE_2D, fbo.color_tex);
        drawMesh(blit_quad_);
    }
}

// --- GPU timer ---
bool GL2Renderer::hasTimerQueries() const {
    return gpu_timer_.available();
}

void GL2Renderer::timerBegin() {
    gpu_timer_.begin();
}

void GL2Renderer::timerEnd() {
    gpu_timer_.end();
}

double GL2Renderer::timerElapsedMs() {
    return gpu_timer_.elapsed_ms();
}

const RenderCaps& GL2Renderer::getCaps() const { return caps_; }
bool GL2Renderer::isCoreProfile() const { return core_profile_; }
const char* GL2Renderer::getGPUVendor()   const { return gpu_vendor_.c_str(); }
const char* GL2Renderer::getGPURenderer() const { return gpu_renderer_.c_str(); }
const char* GL2Renderer::getGLVersion()   const { return gl_version_.c_str(); }
const char* GL2Renderer::getRendererName() const { return "GL2"; }
