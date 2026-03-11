#include "gl2_renderer.h"
#include <cstdio>
#include <cstring>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <SDL_syswm.h>
#include <dirent.h>
#endif

// ---- Embedded GLSL 1.20 shaders ----

static const char* VS_3D =
    "#version 120\n"
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_normal;\n"
    "attribute vec2 a_uv;\n"
    "uniform mat4 u_proj;\n"
    "uniform mat4 u_view;\n"
    "uniform mat4 u_model;\n"
    "varying vec3 v_normal;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_normal = mat3(u_model) * a_normal;\n"
    "    v_uv = a_uv;\n"
    "    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);\n"
    "}\n";

static const char* FS_3D =
    "#version 120\n"
    "varying vec3 v_normal;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform float u_use_tex;\n"
    "uniform vec4 u_color;\n"
    "uniform vec3 u_light_dir;\n"
    "void main() {\n"
    "    vec3 n = normalize(v_normal);\n"
    "    float d = max(dot(n, normalize(u_light_dir)), 0.0);\n"
    "    float light = 0.15 + 0.85 * d;\n"
    "    vec4 c = u_color;\n"
    "    if (u_use_tex > 0.5)\n"
    "        c = texture2D(u_tex, v_uv);\n"
    "    gl_FragColor = vec4(c.rgb * light, c.a);\n"
    "}\n";

static const char* VS_2D =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char* FS_2D_COLOR =
    "#version 120\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "    gl_FragColor = u_color;\n"
    "}\n";

static const char* VS_2D_TEX =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char* FS_2D_TEX =
    "#version 120\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_tex, v_uv);\n"
    "}\n";

// ---- Implementation ----

GL2Renderer::GL2Renderer() : current_shader_(0), initialized_(false) {
    memset(&shader_3d_, 0, sizeof(shader_3d_));
    memset(&shader_2d_color_, 0, sizeof(shader_2d_color_));
    memset(&shader_2d_tex_, 0, sizeof(shader_2d_tex_));
}

GL2Renderer::~GL2Renderer() {
    shutdown();
}

GLuint GL2Renderer::compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, 0);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), 0, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint GL2Renderer::linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
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
        glGetProgramInfoLog(p, sizeof(log), 0, log);
        fprintf(stderr, "Program link error: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

bool GL2Renderer::buildShader(ShaderProg& prog, const char* vs_src, const char* fs_src) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fs_src);
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
    #define GL_MAX_VERTEX_ATTRIBS 0x8869
    #endif
    caps_.max_vertex_attribs = 8;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &caps_.max_vertex_attribs);
    if (caps_.max_vertex_attribs < 8) caps_.max_vertex_attribs = 8;

    // Desktop GL always supports GL_UNSIGNED_INT indices since GL 1.1.
    caps_.supports_32bit_indices = true;

    // Parse GL version
    const char* ver = (const char*)glGetString(GL_VERSION);
    caps_.gl_major = 2;
    caps_.gl_minor = 0;
    if (ver) {
        int major = 0, minor = 0;
        if (sscanf(ver, "%d.%d", &major, &minor) >= 2) {
            caps_.gl_major = major;
            caps_.gl_minor = minor;
        }
    }

    // Detect capabilities via function pointers first, extension strings as fallback.
    caps_.has_vao = false;
    caps_.has_instancing = false;
    caps_.has_generate_mipmap_func = false;
    caps_.has_timer_queries = false;
    caps_.has_fbo = false;

    // VAO: check function pointers (imgl3w or link-time)
#ifdef CB_NEED_GL_LOAD
    caps_.has_vao = (imgl3wProcs.gl.GenVertexArrays != 0)
                 && (imgl3wProcs.gl.BindVertexArray != 0)
                 && (imgl3wProcs.gl.DeleteVertexArrays != 0);
    caps_.has_instancing = (cb_glDrawElementsInstanced != 0)
                        && (cb_glVertexAttribDivisor != 0);
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
        caps_.has_vao = true;
        caps_.has_generate_mipmap_func = true;
        caps_.has_fbo = true;
        if (caps_.gl_major > 3 || (caps_.gl_major == 3 && caps_.gl_minor >= 1))
            caps_.has_instancing = true;
        if (caps_.gl_major > 3 || (caps_.gl_major == 3 && caps_.gl_minor >= 3))
            caps_.has_timer_queries = true;
    }
    const char* exts = (const char*)glGetString(GL_EXTENSIONS);
    if (exts) {
        if (!caps_.has_vao && strstr(exts, "GL_ARB_vertex_array_object"))
            caps_.has_vao = true;
        if (!caps_.has_instancing && strstr(exts, "GL_ARB_instanced_arrays"))
            caps_.has_instancing = true;
        if (!caps_.has_generate_mipmap_func && strstr(exts, "GL_ARB_framebuffer_object"))
            caps_.has_generate_mipmap_func = true;
        if (!caps_.has_fbo && strstr(exts, "GL_ARB_framebuffer_object"))
            caps_.has_fbo = true;
        if (!caps_.has_fbo && strstr(exts, "GL_EXT_framebuffer_object"))
            caps_.has_fbo = true;
        if (!caps_.has_timer_queries && strstr(exts, "GL_ARB_timer_query"))
            caps_.has_timer_queries = true;
    }
#endif

    // Try to detect VRAM. Multiple fallback methods for different drivers.
    caps_.estimated_vram_mb = 0;
#ifndef __APPLE__
    #define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX 0x9048
    #define GL_TEXTURE_FREE_MEMORY_ATI 0x87FC

    const char* exts_vram = (const char*)glGetString(GL_EXTENSIONS);
    if (exts_vram) {
        // Method 1: NVIDIA proprietary driver
        if (strstr(exts_vram, "GL_NVX_gpu_memory_info")) {
            GLint kb = 0;
            glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &kb);
            if (kb > 0) caps_.estimated_vram_mb = kb / 1024;
        }
        // Method 2: AMD proprietary / Mesa RadeonSI (sometimes)
        if (caps_.estimated_vram_mb == 0 && strstr(exts_vram, "GL_ATI_meminfo")) {
            GLint info[4] = {0};
            glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, info);
            if (info[0] > 0) caps_.estimated_vram_mb = info[0] / 1024;
        }
    }

#ifndef _WIN32
    // Method 3: GLX_MESA_query_renderer (Mesa: Nouveau, RadeonSI, Intel, llvmpipe, Zink)
    if (caps_.estimated_vram_mb == 0) {
        #define GLX_RENDERER_VIDEO_MEMORY_MESA 0x8187
        typedef int (*PFNGLXQUERYRENDERERMESA)(int, int, unsigned int*);
        PFNGLXQUERYRENDERERMESA queryRenderer =
            (PFNGLXQUERYRENDERERMESA)SDL_GL_GetProcAddress("glXQueryCurrentRendererIntegerMESA");
        if (queryRenderer) {
            unsigned int vram_mb = 0;
            if (queryRenderer(GLX_RENDERER_VIDEO_MEMORY_MESA, 0, &vram_mb) && vram_mb > 0) {
                caps_.estimated_vram_mb = (int)vram_mb;
            }
        }
    }

    // Method 4: Linux sysfs — scan /sys/class/drm/card*/device/mem_info_vram_total (AMD)
    // and /sys/class/drm/card*/device/resource (NVIDIA / other PCI devices)
    if (caps_.estimated_vram_mb == 0) {
        DIR* drm_dir = opendir("/sys/class/drm");
        if (drm_dir) {
            struct dirent* entry;
            while ((entry = readdir(drm_dir)) != NULL) {
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
                        caps_.estimated_vram_mb = (int)(bytes / (1024 * 1024));
                    }
                    fclose(f);
                    if (caps_.estimated_vram_mb > 0) break;
                }
            }
            closedir(drm_dir);
        }
    }
#endif // !_WIN32
#endif // !__APPLE__

    fprintf(stderr, "GL Caps: GL %d.%d, max_tex=%d, max_attribs=%d, vram=%dMB, "
            "vao=%s, instancing=%s, fbo=%s, timer_q=%s\n",
            caps_.gl_major, caps_.gl_minor,
            caps_.max_texture_size, caps_.max_vertex_attribs,
            caps_.estimated_vram_mb,
            caps_.has_vao ? "yes" : "no",
            caps_.has_instancing ? "yes" : "no",
            caps_.has_fbo ? "yes" : "no",
            caps_.has_timer_queries ? "yes" : "no");
}

bool GL2Renderer::init(int w, int h) {
    if (initialized_) return true;

    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version = (const char*)glGetString(GL_VERSION);
    gpu_vendor_   = vendor   ? vendor   : "Unknown";
    gpu_renderer_ = renderer ? renderer : "Unknown";
    gl_version_   = version  ? version  : "Unknown";

    detectCaps();

    if (!buildShader(shader_3d_, VS_3D, FS_3D)) return false;
    if (!buildShader(shader_2d_color_, VS_2D, FS_2D_COLOR)) return false;
    if (!buildShader(shader_2d_tex_, VS_2D_TEX, FS_2D_TEX)) return false;

    // Slot 0 for all meshes/textures/custom_shaders is reserved as "invalid"
    meshes_.push_back(GLMesh());
    meshes_[0].valid = false;
    textures_.push_back(GLTex());
    textures_[0].valid = false;
    custom_shaders_.push_back(0); // slot 0 = invalid

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Set sensible default uniforms for all shaders
    ShaderProg* shaders[] = {&shader_3d_, &shader_2d_color_, &shader_2d_tex_};
    for (int i = 0; i < 3; i++) {
        ShaderProg* sp = shaders[i];
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

    if (shader_3d_.program) { glDeleteProgram(shader_3d_.program); shader_3d_.program = 0; }
    if (shader_2d_color_.program) { glDeleteProgram(shader_2d_color_.program); shader_2d_color_.program = 0; }
    if (shader_2d_tex_.program) { glDeleteProgram(shader_2d_tex_.program); shader_2d_tex_.program = 0; }

    current_shader_ = 0;
    initialized_ = false;
}

void GL2Renderer::setViewport(int x, int y, int w, int h) {
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
    gm.index_count = (int)data.indices.size();

    glGenBuffers(1, &gm.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gm.vbo);
    glBufferData(GL_ARRAY_BUFFER, data.vertices.size() * sizeof(Vertex),
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
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.indices.size() * sizeof(unsigned int),
                     data.indices.data(), GL_STATIC_DRAW);
    } else if (needs_32bit) {
        fprintf(stderr, "ERROR: mesh requires 32-bit indices but hardware doesn't support them\n");
        glDeleteBuffers(1, &gm.vbo);
        glDeleteBuffers(1, &gm.ibo);
        return INVALID_MESH;
    } else {
        gm.index_type = GL_UNSIGNED_SHORT;
        std::vector<unsigned short> indices16(data.indices.size());
        for (size_t i = 0; i < data.indices.size(); i++)
            indices16[i] = (unsigned short)data.indices[i];
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices16.size() * sizeof(unsigned short),
                     indices16.data(), GL_STATIC_DRAW);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    MeshHandle h;
    if (!free_mesh_slots_.empty()) {
        h = free_mesh_slots_.back();
        free_mesh_slots_.pop_back();
        meshes_[h] = gm;
    } else {
        h = (MeshHandle)meshes_.size();
        meshes_.push_back(gm);
    }
    return h;
}

void GL2Renderer::destroyMesh(MeshHandle h) {
    if (h == 0 || h >= meshes_.size() || !meshes_[h].valid) return;
    glDeleteBuffers(1, &meshes_[h].vbo);
    glDeleteBuffers(1, &meshes_[h].ibo);
    meshes_[h].valid = false;
    free_mesh_slots_.push_back(h);
}

TextureHandle GL2Renderer::createTexture(int w, int h, int channels, const unsigned char* pixels) {
    if (w > caps_.max_texture_size || h > caps_.max_texture_size) {
        fprintf(stderr, "Warning: texture %dx%d exceeds max %d, clamping\n",
                w, h, caps_.max_texture_size);
        int nw = w, nh = h;
        while (nw > caps_.max_texture_size) nw /= 2;
        while (nh > caps_.max_texture_size) nh /= 2;

        std::vector<unsigned char> scaled(nw * nh * channels);
        for (int sy = 0; sy < nh; sy++) {
            for (int sx = 0; sx < nw; sx++) {
                int src_x = sx * w / nw;
                int src_y = sy * h / nh;
                int dst_idx = (sy * nw + sx) * channels;
                int src_idx = (src_y * w + src_x) * channels;
                for (int c = 0; c < channels; c++)
                    scaled[dst_idx + c] = pixels[src_idx + c];
            }
        }
        return createTexture(nw, nh, channels, scaled.data());
    }

    GLTex gt;
    gt.valid = true;
    glGenTextures(1, &gt.id);
    glBindTexture(GL_TEXTURE_2D, gt.id);

    // GL 1.4 auto mipmap generation (works on GL 2.1 without glGenerateMipmap)
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GLenum fmt = (channels == 4) ? GL_RGBA : (channels == 3) ? GL_RGB : GL_LUMINANCE;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, pixels);

    glBindTexture(GL_TEXTURE_2D, 0);

    TextureHandle th;
    if (!free_tex_slots_.empty()) {
        th = free_tex_slots_.back();
        free_tex_slots_.pop_back();
        textures_[th] = gt;
    } else {
        th = (TextureHandle)textures_.size();
        textures_.push_back(gt);
    }
    return th;
}

void GL2Renderer::destroyTexture(TextureHandle h) {
    if (h == 0 || h >= textures_.size() || !textures_[h].valid) return;
    glDeleteTextures(1, &textures_[h].id);
    textures_[h].valid = false;
    free_tex_slots_.push_back(h);
}

void GL2Renderer::useShader(ShaderType type) {
    switch (type) {
        case SHADER_3D:          current_shader_ = &shader_3d_; break;
        case SHADER_2D_COLOR:    current_shader_ = &shader_2d_color_; break;
        case SHADER_2D_TEXTURED: current_shader_ = &shader_2d_tex_; break;
    }
    glUseProgram(current_shader_->program);
}

ShaderHandle GL2Renderer::createCustomShader(const char* vs_src, const char* fs_src) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return INVALID_SHADER;
    }
    GLuint prog = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!prog) return INVALID_SHADER;

    ShaderHandle h;
    if (!free_custom_slots_.empty()) {
        h = free_custom_slots_.back();
        free_custom_slots_.pop_back();
        custom_shaders_[h] = prog;
    } else {
        h = (ShaderHandle)custom_shaders_.size();
        custom_shaders_.push_back(prog);
    }
    return h;
}

void GL2Renderer::useCustomShader(ShaderHandle h) {
    if (h == 0 || h >= custom_shaders_.size() || !custom_shaders_[h]) return;
    current_shader_ = 0; // no built-in shader active
    glUseProgram(custom_shaders_[h]);
}

void GL2Renderer::destroyCustomShader(ShaderHandle h) {
    if (h == 0 || h >= custom_shaders_.size() || !custom_shaders_[h]) return;
    glDeleteProgram(custom_shaders_[h]);
    custom_shaders_[h] = 0;
    free_custom_slots_.push_back(h);
}

int GL2Renderer::getCustomUniformLoc(ShaderHandle h, const char* name) {
    if (h == 0 || h >= custom_shaders_.size() || !custom_shaders_[h]) return -1;
    return glGetUniformLocation(custom_shaders_[h], name);
}

void GL2Renderer::setUniform1i(int loc, int v) {
    if (loc >= 0) glUniform1i(loc, v);
}

void GL2Renderer::setUniform1f(int loc, float v) {
    if (loc >= 0) glUniform1f(loc, v);
}

void GL2Renderer::setUniform4f(int loc, float r, float g, float b, float a) {
    if (loc >= 0) glUniform4f(loc, r, g, b, a);
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
    if (h == 0 || h >= textures_.size() || !textures_[h].valid) {
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_[h].id);
    if (current_shader_ && current_shader_->u_tex >= 0)
        glUniform1i(current_shader_->u_tex, 0);
}

void GL2Renderer::setUseTexture(bool use) {
    if (current_shader_ && current_shader_->u_use_tex >= 0)
        glUniform1f(current_shader_->u_use_tex, use ? 1.0f : 0.0f);
}

void GL2Renderer::drawMesh(MeshHandle h) {
    if (h == 0 || h >= meshes_.size() || !meshes_[h].valid) return;
    const GLMesh& gm = meshes_[h];

    glBindBuffer(GL_ARRAY_BUFFER, gm.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm.ibo);

    GLsizei stride = sizeof(Vertex);

    // For custom shaders, use fixed attribute locations 0, 1, 2
    GLint loc_pos = 0, loc_normal = 1, loc_uv = 2;
    if (current_shader_) {
        loc_pos    = current_shader_->a_pos;
        loc_normal = current_shader_->a_normal;
        loc_uv     = current_shader_->a_uv;
    }

    if (loc_pos >= 0) {
        glEnableVertexAttribArray(loc_pos);
        glVertexAttribPointer(loc_pos, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    }
    if (loc_normal >= 0) {
        glEnableVertexAttribArray(loc_normal);
        glVertexAttribPointer(loc_normal, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    }
    if (loc_uv >= 0) {
        glEnableVertexAttribArray(loc_uv);
        glVertexAttribPointer(loc_uv, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    }

    glDrawElements(GL_TRIANGLES, gm.index_count, gm.index_type, 0);

    if (loc_pos >= 0)    glDisableVertexAttribArray(loc_pos);
    if (loc_normal >= 0) glDisableVertexAttribArray(loc_normal);
    if (loc_uv >= 0)     glDisableVertexAttribArray(loc_uv);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GL2Renderer::uploadTextureData(TextureHandle h, int w, int h_, int channels, const unsigned char* pixels) {
    if (h == 0 || h >= textures_.size() || !textures_[h].valid) return;
    glBindTexture(GL_TEXTURE_2D, textures_[h].id);
    GLenum fmt = (channels == 4) ? GL_RGBA : (channels == 3) ? GL_RGB : GL_LUMINANCE;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h_, fmt, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GL2Renderer::setColorMask(bool r, bool g, bool b, bool a) {
    glColorMask(r ? GL_TRUE : GL_FALSE, g ? GL_TRUE : GL_FALSE,
                b ? GL_TRUE : GL_FALSE, a ? GL_TRUE : GL_FALSE);
}

void GL2Renderer::setBlending(bool enable) {
    if (enable) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

void GL2Renderer::setDepthTest(bool enable) {
    if (enable)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
}

void GL2Renderer::resetState() {
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    current_shader_ = 0;
}

const RenderCaps& GL2Renderer::getCaps() const { return caps_; }
const char* GL2Renderer::getGPUVendor()   const { return gpu_vendor_.c_str(); }
const char* GL2Renderer::getGPURenderer() const { return gpu_renderer_.c_str(); }
const char* GL2Renderer::getGLVersion()   const { return gl_version_.c_str(); }
const char* GL2Renderer::getRendererName() const { return "GL2"; }
