#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <assert.h>
#include <dlfcn.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h>
#include <gbm.h>
#include <linux/fb.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <limits.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <sstream>
#include <thread>
#include <unordered_map>

#include <wlr/render/interface.h>
#include <wlr/render/wlr_renderer.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_shm.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

template <typename... Args>
void log_info(Args... args) {
  ((std::cout << " " << args), ...) << std::endl;
}
template <typename... Args>
void log_err(Args... args) {
  ((std::cerr << " " << args), ...) << std::endl;
}
#ifdef NDEBUG
#define log_debug(...) ((void)0)
#else
template <typename... Args>
void log_debug(Args... args) {
  log_info(args...);
}
#endif

struct EGL {
  // getting address of extenstions for surface initializing with non default platform
  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLBINDWAYLANDDISPLAYWL)(EGLDisplay dpy, struct wl_display *display);
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLUNBINDWAYLANDDISPLAYWL)(EGLDisplay dpy, struct wl_display *display);
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL)(EGLDisplay dpy, struct wl_resource *buffer,
                                                              EGLint attribute, EGLint *value);
  typedef EGLDisplay(EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXTPROC)(EGLenum platform, void *native_display,
                                                                   const EGLint *attrib_list);
  typedef EGLSurface(EGLAPIENTRYP PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)(EGLDisplay display, EGLConfig config,
                                                                            void *native_window,
                                                                            const EGLint *attrib_list);

  PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
  PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT;
  PFNEGLBINDWAYLANDDISPLAYWL bindDisplay;
  PFNEGLUNBINDWAYLANDDISPLAYWL unbindDisplay;
  PFNEGLQUERYWAYLANDBUFFERWL queryBuffer;
  EGL() {
    const char *clientExtensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    log_info("available EGL extensions", clientExtensions);
    eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    glEGLImageTargetTexture2DOES =
        reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    bindDisplay = reinterpret_cast<PFNEGLBINDWAYLANDDISPLAYWL>(eglGetProcAddress("eglBindWaylandDisplayWL"));
    queryBuffer = reinterpret_cast<PFNEGLQUERYWAYLANDBUFFERWL>(eglGetProcAddress("eglQueryWaylandBufferWL"));
    unbindDisplay = reinterpret_cast<PFNEGLUNBINDWAYLANDDISPLAYWL>(eglGetProcAddress("eglUnbindWaylandDisplayWL"));
    eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    eglCreatePlatformWindowSurfaceEXT =
        (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
  }

  static GLuint createShader(const char *source, GLenum shaderType) {
    if (!source) {
      log_err("empty shader obtained");
      return 0;
    }
    GLuint shader = 0u;
    GLint status = 0;

    shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
      char log[1000];
      GLsizei len;
      glGetShaderInfoLog(shader, 1000, &len, log);
      log_err((shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment"), "shader compilation failed:", log);
      return 0u;
    }
    return shader;
  }

  EGLDisplay eglDisplay{EGL_NO_DISPLAY};
  EGLConfig eglConfig{EGL_NO_CONFIG_KHR};
  EGLSurface eglSurface{EGL_NO_SURFACE};
  EGLContext eglContext{EGL_NO_CONTEXT};

  inline static const EGLint configAttributes[] = {
      EGL_BUFFER_SIZE,
      32,
      EGL_ALPHA_SIZE,
      8,
      EGL_BLUE_SIZE,
      8,
      EGL_GREEN_SIZE,
      8,
      EGL_RED_SIZE,
      8,
      EGL_DEPTH_SIZE,
      24,
      EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE,
      EGL_WINDOW_BIT,
      EGL_NONE,
  };
};

/**
 * Our Wayland server supports the next interfaces: dmabuf, xdg_shell, wl_shm
 * Wayland server runs on own thread with separate dummy GL context using GBM based dummy surface 2x2 pixels
 * to satisfy wayland clients since they use DMA mechanism for GBM buffers to export pixels to our server
 * and in turn our server imports DMA buffers
 * running wayland loop processes wayland clients requests, grabs client's surfaces, requests clients about new frames
 */
struct WaylandServer : wlr_renderer, wl_listener {
  struct Texture {
    const EGL &egl;
    GLuint textureId{0u};
    EGLImageKHR image{EGL_NO_IMAGE_KHR};
    int width{0};
    int height{0};

    Texture(const EGL &_egl, int _width, int _height) : egl(_egl), width(_width), height(_height) {
      glGenTextures(1, &textureId);
      log_debug("Generated texture id", textureId, "width", width, "height", height);
      glBindTexture(GL_TEXTURE_2D, textureId);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    Texture(const Texture &) = delete;
    Texture(Texture &&) = delete;
    ~Texture() {
      log_debug("~Texture");
      if (textureId) {
        glDeleteTextures(1, &textureId);
      }
      if (image != EGL_NO_IMAGE_KHR) {
        egl.eglDestroyImageKHR(egl.eglDisplay, image);
      }
    }
  };
  struct ClientSurface : wlr_texture {
    wlr_dmabuf_attributes dmabuf{.width = 0, .height = 0, .n_planes = 0};
    void *sharedMem{nullptr};
    std::shared_ptr<WaylandServer::Texture> texture{nullptr};
    ~ClientSurface() { log_debug("~ClientSurface"); }
  };
  typedef void (*wlroots_init_log)(enum wlr_log_importance verbosity, wlr_log_func_t callback);
  typedef wlr_shm *(*wlroots_shm_create)(struct wl_display *display, uint32_t version, const uint32_t *formats,
                                         size_t formats_len);
  typedef wlr_xdg_shell *(*wlroots_xdg_shell_create)(struct wl_display *display, uint32_t version);
  typedef void *(*wlroots_renderer_init)(struct wlr_renderer *renderer, const struct wlr_renderer_impl *impl, uint32_t render_buffer_caps);
  typedef wlr_linux_dmabuf_v1 *(*wlroots_dma_init)(struct wl_display *display, uint32_t version,
                                                   struct wlr_renderer *renderer);
  typedef wlr_compositor *(*wlroots_compositor_create)(struct wl_display *display, uint32_t version,
                                                       struct wlr_renderer *renderer);
  typedef bool (*wlroots_buffer_get_dmabuf)(struct wlr_buffer *buffer, struct wlr_dmabuf_attributes *attribs);
  typedef bool (*wlroots_buffer_begin_data_ptr_access)(struct wlr_buffer *buffer, uint32_t flags, void **data,
                                                       uint32_t *format, size_t *stride);
  typedef bool (*wlroots_buffer_end_data_ptr_access)(struct wlr_buffer *buffer);
  typedef bool (*wlroots_drm_format_set_add)(struct wlr_drm_format_set *set, uint32_t format, uint64_t modifier);
  typedef void (*wlroots_drm_format_set_finish)(struct wlr_drm_format_set *set);
  typedef wlr_texture *(*wlroots_get_texture_from_surface)(struct wlr_surface *surface);
  typedef void *(*wlroots_texture_init)(struct wlr_texture *texture, struct wlr_renderer *renderer,
                                        const struct wlr_texture_impl *impl, uint32_t width, uint32_t height);
  typedef void *(*wlroots_send_frame_done)(struct wlr_surface *surface, const struct timespec *when);
  typedef wlr_viewporter *(*wlroots_viewporter_create)(struct wl_display *display);
  typedef wlr_xdg_toplevel *(*wlroots_check_surface)(struct wlr_surface *surface);

  wlroots_buffer_get_dmabuf getDmaBuf{nullptr};
  wlroots_buffer_begin_data_ptr_access beginBufAccess{nullptr};
  wlroots_buffer_end_data_ptr_access endBufAccess{nullptr};
  wlroots_get_texture_from_surface getTextureFromSurface{nullptr};
  wlroots_texture_init initTexture{nullptr};
  wlroots_dma_init initDma{nullptr};
  wlroots_drm_format_set_finish freeFormats{nullptr};
  wlroots_compositor_create createCompositor{nullptr};
  wlroots_send_frame_done sendFrameDoneForClient{nullptr};
  wlroots_viewporter_create createViewporter{nullptr};
  wlroots_check_surface isSurfaceOperable{nullptr};

  // clients surfaces with bool indicating whether the surface is above content and produced GL texture
  inline static std::unordered_map<wlr_surface *, std::pair<bool, ClientSurface *>> _surfaces;
  // information about texture creation in render thread from client surface got in wayland server thread
  inline static std::atomic<bool> _isExternalTextureProcessing = true;
  inline static std::atomic<bool> _isSurfaceInvalidated = false;
  inline static wl_listener _onSurfaceRemoved{.notify = [](wl_listener *listener, void *data) {
    log_debug("some surface lost");
    _isSurfaceInvalidated = true;
  }};

  const EGL &egl;

  const int& drmDevice;

  const std::array<unsigned int, 2u> formats{DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888};
  wlr_drm_format_set drmFormats{};

  wl_display *waylandDisplay{nullptr};
  wl_event_loop *waylandLoop{nullptr};

  // for surfaces passed over shared buffer (we don't have such clients but we support wl_shm protocol)
  uint8_t *mappedMemory{nullptr};
  size_t mappedMemoryBytes{0u};

  const wlr_renderer_impl rendererImpl {
      .get_texture_formats = [](wlr_renderer *renderer, uint32_t buffer_caps) -> const wlr_drm_format_set * {
        return &static_cast<WaylandServer *>(renderer)->drmFormats;
      },
      .get_render_formats = [](wlr_renderer *renderer) -> const wlr_drm_format_set * {
        return &static_cast<WaylandServer *>(renderer)->drmFormats;
      },
      .destroy = [](wlr_renderer *renderer) { delete renderer; },
      .get_drm_fd = [](struct wlr_renderer *renderer) -> int {
        return static_cast<WaylandServer *>(renderer)->drmDevice;
      },
      .texture_from_buffer = [](wlr_renderer *renderer, wlr_buffer *buffer) -> wlr_texture * {
        log_debug(buffer, "texture extraction");
        static wlr_texture_impl commonTextureImpl{
            .destroy = [](wlr_texture *texture) { delete static_cast<ClientSurface *>(texture); }};
        WaylandServer *wlServer = static_cast<WaylandServer *>(renderer);
        assert(wlServer->getDmaBuf && wlServer->beginBufAccess && wlServer->endBufAccess);

        ClientSurface *texture = new ClientSurface();
        texture->impl = &commonTextureImpl;
        texture->width = buffer->width;
        texture->height = buffer->height;

        uint32_t format;
        size_t stride;
        void *sharedMem{nullptr};

        if (wlServer->getDmaBuf(buffer, &texture->dmabuf)) {
          log_debug("dma buf added");
        } else if (wlServer->beginBufAccess(buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ, &sharedMem, &format, &stride)) {
          log_debug("shmem buf added");
          size_t requestedBytesAmount = 4 * texture->width * texture->height;  // 4 bytes per pixel
          if (wlServer->mappedMemoryBytes < requestedBytesAmount) {
            wlServer->mappedMemory = (uint8_t *)realloc(wlServer->mappedMemory, requestedBytesAmount);
          }
          // we temporarily acquire ownership to memory block and store it
          // Nore: Can be reconsidered to create texture in place if we have real wl_shm clients on TV
          // since egl-dma outperformes this protocol very likely it will never be used by real clients on TV
          memcpy(wlServer->mappedMemory, sharedMem, requestedBytesAmount);
          texture->sharedMem = (void *)wlServer->mappedMemory;
          wlServer->endBufAccess(buffer);
        }

        return texture;
      },
      .begin_buffer_pass = [](wlr_renderer *renderer, wlr_buffer *buffer, const wlr_buffer_pass_options *options) -> wlr_render_pass * { return nullptr; },
      .render_timer_create = [](wlr_renderer *renderer) -> wlr_render_timer * { return nullptr; }
    };

  WaylandServer(const EGL &_egl, int& _drmDevice)
      : wl_listener{.notify =
                        [](wl_listener *listener, void *data) {
                          wlr_xdg_surface *surface = static_cast<wlr_xdg_surface *>(data);
                          log_info("New Surface is null:", (surface == nullptr));
                          wl_signal_add(&surface->events.destroy, &_onSurfaceRemoved);
                          bool isAbove = false;
                          auto topLevel = surface->toplevel;
                          if (topLevel) {
                            log_info("surface title", topLevel->title);
                          }
                          _surfaces.insert({surface->surface, {isAbove, nullptr}});
                        }},
        egl(_egl),
        drmDevice(_drmDevice)  {
    if (drmDevice < 0) {
      log_err("invalid drm device");
      std::exit(EXIT_FAILURE);
    }

    void *handle = dlopen("/usr/lib/libwlroots-0.19.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
      log_err("Unable to open wlroots library", dlerror());
      std::exit(EXIT_FAILURE);
    }

    wlroots_init_log initLog = (wlroots_init_log)dlsym(handle, "wlr_log_init");
    log_debug("address of initLog is valid: ", initLog != nullptr ? "OK" : dlerror());
    wlroots_shm_create initSharedMem = (wlroots_shm_create)dlsym(handle, "wlr_shm_create");
    log_debug("address of initSharedMem is valid: ", initSharedMem != nullptr ? "OK" : dlerror());
    wlroots_xdg_shell_create initXdgShellProtocol = (wlroots_xdg_shell_create)dlsym(handle, "wlr_xdg_shell_create");
    log_debug("address of initXdgShellProtocol is valid: ", initXdgShellProtocol != nullptr ? "OK" : dlerror());
    wlroots_renderer_init initRenderer = (wlroots_renderer_init)dlsym(handle, "wlr_renderer_init");
    log_debug("address of initRenderer is valid: ", initRenderer != nullptr ? "OK" : dlerror());
    wlroots_drm_format_set_add addDrmFormat = (wlroots_drm_format_set_add)dlsym(handle, "wlr_drm_format_set_add");
    log_debug("address of addDrmFormat is valid: ", addDrmFormat != nullptr ? "OK" : dlerror());
    initDma = (wlroots_dma_init)dlsym(handle, "wlr_linux_dmabuf_v1_create_with_renderer");
    log_debug("address of initDma is valid: ", initDma != nullptr ? "OK" : dlerror());
    freeFormats = (wlroots_drm_format_set_finish)dlsym(handle, "wlr_drm_format_set_finish");
    log_debug("address of freeFormats is valid: ", freeFormats != nullptr ? "OK" : dlerror());
    createCompositor = (wlroots_compositor_create)dlsym(handle, "wlr_compositor_create");
    log_debug("address of createCompositor is valid: ", createCompositor != nullptr ? "OK" : dlerror());
    getDmaBuf = (wlroots_buffer_get_dmabuf)dlsym(handle, "wlr_buffer_get_dmabuf");
    log_debug("address of getDmaBuf is valid: ", getDmaBuf != nullptr ? "OK" : dlerror());
    beginBufAccess = (wlroots_buffer_begin_data_ptr_access)dlsym(handle, "wlr_buffer_begin_data_ptr_access");
    log_debug("address of beginBufAccess is valid: ", beginBufAccess != nullptr ? "OK" : dlerror());
    endBufAccess = (wlroots_buffer_end_data_ptr_access)dlsym(handle, "wlr_buffer_end_data_ptr_access");
    log_debug("address of endBufAccess is valid: ", endBufAccess != nullptr ? "OK" : dlerror());
    getTextureFromSurface = (wlroots_get_texture_from_surface)dlsym(handle, "wlr_surface_get_texture");
    log_debug("address of getTextureFromSurface is valid: ", getTextureFromSurface != nullptr ? "OK" : dlerror());
    initTexture = (wlroots_texture_init)dlsym(handle, "wlr_texture_init");
    log_debug("address of initTexture is valid: ", initTexture != nullptr ? "OK" : dlerror());
    sendFrameDoneForClient = (wlroots_send_frame_done)dlsym(handle, "wlr_surface_send_frame_done");
    log_debug("address of sendFrameDoneForClient is valid: ", sendFrameDoneForClient != nullptr ? "OK" : dlerror());
    createViewporter = (wlroots_viewporter_create)dlsym(handle, "wlr_viewporter_create");
    log_debug("address of createViewporter is valid: ", createViewporter != nullptr ? "OK" : dlerror());
    isSurfaceOperable = (wlroots_check_surface)dlsym(handle, "wlr_xdg_toplevel_try_from_wlr_surface");
    log_debug("address of isSurfaceOperable is valid: ", isSurfaceOperable != nullptr ? "OK" : dlerror());

    if (!initLog || !initSharedMem || !initXdgShellProtocol || !initRenderer || !addDrmFormat || !initDma ||
        !freeFormats || !createCompositor || !getDmaBuf || !beginBufAccess || !endBufAccess || !getTextureFromSurface ||
        !initTexture || !sendFrameDoneForClient || !createViewporter || !isSurfaceOperable) {
      log_err("Unable to get the address of the wlroots lib function");
      std::exit(EXIT_FAILURE);
    }

    initLog(WLR_DEBUG, NULL);

    /* The Wayland display is managed by libwayland. It handles accepting
     * clients from the Unix socket, managing Wayland global vars, and so on. */
    waylandDisplay = wl_display_create();

    addDrmFormat(&drmFormats, formats[0], 0);
    addDrmFormat(&drmFormats, formats[1], 0);
    initSharedMem(waylandDisplay, 1, formats.data(), formats.size());  // wl_shm interface ver 1

    auto xdgShell = initXdgShellProtocol(waylandDisplay, 3);  // xdg_shell interface ver 3
    wl_signal_add(&xdgShell->events.new_surface, static_cast<wl_listener *>(this));

    initRenderer(static_cast<wlr_renderer *>(this), &rendererImpl, WLR_BUFFER_CAP_DMABUF);

    waylandLoop = wl_display_get_event_loop(waylandDisplay);
    assert(waylandLoop);

    if (egl.bindDisplay && egl.unbindDisplay && egl.queryBuffer) {
      initDma(waylandDisplay, 4, static_cast<wlr_renderer *>(this));  // dmaduf interface ver 4
      // egl in libamli(or mesa) uses mali_buffer_sharing(or mesa alterantive) for graphics buffers sharing
      // report mali_buffer_sharing as supported, as without that interface only
      // wl_shm could be used
      log_info("Mali or Mesa buffer sharing is supported");
      if (!egl.bindDisplay(egl.eglDisplay, waylandDisplay)) {
        log_err("eglBindWaylandDisplayWL call failed");
        std::exit(EXIT_FAILURE);
      }
    } else {
      log_err("Mali or Mesa buffer sharing is not supported!");
      std::exit(EXIT_FAILURE);
    }

    createViewporter(waylandDisplay);  // wp_viewporter interface ver 1

    createCompositor(waylandDisplay, 5, static_cast<wlr_renderer *>(this));  // wayland server ver 5

    auto socket = wl_display_add_socket_auto(waylandDisplay);
    if (!socket) {
      log_err("adding Unix socket failed");
      std::exit(EXIT_FAILURE);
    }
    setenv("WAYLAND_DISPLAY", socket, true);

    log_info("Running Wayland compositor on WAYLAND_DISPLAY=", socket);
  }

  void pollWaylandEvents() {
      // validation of surfaces
      if (_isSurfaceInvalidated) {
        for (auto it = WaylandServer::_surfaces.begin(); it != WaylandServer::_surfaces.end();) {
          if (!isSurfaceOperable(it->first)) {
            it = WaylandServer::_surfaces.erase(it);
            _isSurfaceInvalidated = false;
          } else {
            it++;
          }
        }
      }

      if (_isExternalTextureProcessing) {
        //isTextureGrabbed = false;
        for (auto &surface : WaylandServer::_surfaces) {
          auto texture = getTextureFromSurface(surface.first);

          std::pair<bool, ClientSurface *> &pair = surface.second;
          if (pair.second != texture) {
            log_debug("IMAGE DATA READY");
            auto clientSurface = static_cast<WaylandServer::ClientSurface *>(texture);
            pair.second = clientSurface;
            //isTextureGrabbed = true;
          }
        }

        //if (isTextureGrabbed) {
        //  log_debug("Wake up the render thread");
        //  _isExternalTextureProcessing = false;
        //} else {
        //  pollWaylandEvents();
        //}
      }

      // interaction with clients finished
      wl_display_flush_clients(waylandDisplay);
      // polling\processing wayland events
      wl_event_loop_dispatch(waylandLoop, 0);
  }

  ~WaylandServer() {
    free(mappedMemory);
    if (egl.unbindDisplay && egl.eglDisplay) {
      egl.unbindDisplay(egl.eglDisplay, waylandDisplay);
    }
    if (freeFormats) {
      freeFormats(&drmFormats);
    }
    if (waylandDisplay) {
      wl_display_destroy_clients(waylandDisplay);
      wl_display_destroy(waylandDisplay);
    }
  }
};

struct FBO {
  GLuint fboId{0u};
  GLuint textureId{0u};

  FBO(int width, int height) {
    log_debug("FBO with resolution: ", width, "x", height);
    glGenFramebuffers(1, &fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);
    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      fboId = 0u;
      log_err("Couldn't create FBO with resolution:", width, "x", height, "status:", status);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  FBO(const FBO &) = delete;
  FBO(FBO &&) = delete;
  ~FBO() {
    log_debug("~FBO");
    if (fboId) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers(1, &fboId);
    }
    if (textureId) {
      glDeleteTextures(1, &textureId);
    }
  }
};

/*
 * our main Window drawing content over DRM
 */
class DrmWindow {
  struct GBM_Buf {
    gbm_bo *bo{nullptr};
    uint32_t handle{UINT_MAX};
    uint32_t pitch{UINT_MAX};
    uint32_t fb{UINT_MAX};

    int32_t& drmDevice;
    gbm_surface* &gbmSurfacePtr;

    GBM_Buf(int32_t& _drmDevice, gbm_surface* &_gbmSurfacePtr) : 
            drmDevice(_drmDevice), gbmSurfacePtr(_gbmSurfacePtr) {
              if (drmDevice < 0 || !_gbmSurfacePtr) {
                log_err("invalid drm config");
                std::exit(EXIT_FAILURE);
              }
            }

    ~GBM_Buf() {
      drmModeRmFB(drmDevice, fb);
      gbm_surface_release_buffer(gbmSurfacePtr, bo);
    }
  };

  int mWidth{1920};
  int mHeight{1080};

  int drmDevice{-1};
  drmModeModeInfo mode;
  drmModeCrtc *crtcPtr{nullptr};
  uint32_t connectorId{UINT_MAX};
  gbm_device *gbmDevicePtr{nullptr};
  gbm_surface *gbmSurfacePtr{nullptr};
  gbm_bo *previousBo{nullptr};
  uint32_t previousFb{UINT_MAX};

  const char *vertShader =
      "attribute vec2 position; \n"
      "attribute vec2 texCoords; \n"
      "varying vec2 fragTexCoord;\n"
      "uniform float yFlipped;\n"
      "void main()\n"
      "{\n"
      "    gl_Position = vec4(position, 0.0, 1.0);\n"
      "    fragTexCoord = texCoords;\n"
      "    fragTexCoord.y = mix(fragTexCoord.y, 1.0 - fragTexCoord.y, yFlipped);\n"
      "}\n";

  const char *fragShader =
      "precision mediump float;\n"
      "uniform sampler2D inputColor;\n"
      "varying vec2 fragTexCoord;\n"
      "void main()\n"
      "{\n"
      "    gl_FragColor = texture2D(inputColor, fragTexCoord);\n"
      "}\n";
  GLuint mFrag{0u};
  GLuint mVert{0u};
  GLuint mProgram{0u};
  GLuint mVerticesVBO{0u};
  GLfloat mTexCoords[8] = {0, 0, 0, 1, 1, 0, 1, 1};
  GLint mYFlippedUniform{0};
  std::unique_ptr<FBO> mFBO{nullptr};  // create fbo for zooming effect

  EGL mEgl;
  std::vector<std::shared_ptr<WaylandServer::Texture>> mExternalTexturesAbove;
  std::vector<std::shared_ptr<WaylandServer::Texture>> mExternalTexturesUnder;
  std::unique_ptr<WaylandServer> mWlServer;
  int mFbDevice{-1};

 public:
  ~DrmWindow() { cleanUp(); }
  DrmWindow(int width, int height) : mWidth(width), mHeight(height) {
    log_info("Create wayland widow with resolution:", mWidth, "x", mHeight);
  }

  void initialize() {
    // legacy fbdev functional
    initFbdev();

    drmDevice = open("/dev/dri/card0", O_RDWR);
    if (drmDevice < 0) {
      log_err("Failed to open DRM device");
      std::exit(EXIT_FAILURE);
    }
    drmSetMaster(drmDevice);
    if (drmIsMaster(drmDevice) != 0) {
      log_info("Couldn't aquire DRM ownership");
    }
    drmModeRes *resources = drmModeGetResources(drmDevice);
    if (resources == nullptr)
    {
        log_err("Unable to get DRM resources");
        std::exit(EXIT_FAILURE);
    }

    drmModeConnector *connector = nullptr;
    for (int i = 0; i < resources->count_connectors; i++)
    {
        connector = drmModeGetConnector(drmDevice, resources->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0)
        {
          if(connector->connector_type == DRM_MODE_CONNECTOR_HDMIA)
             log_info("DRM connection over HDMA found");
          else if(connector->connector_type == DRM_MODE_CONNECTOR_HDMIB)
             log_info("DRM connection over HDMB found");
          break;
        }
        drmModeFreeConnector(connector);
    }
    if (connector == nullptr)
    {
        log_err("Unable to get connector");
        drmModeFreeResources(resources);
        std::exit(EXIT_FAILURE);
    }

    connectorId = connector->connector_id;
    mode = connector->modes[0];
    mWidth = mode.hdisplay;
    mHeight = mode.vdisplay;
    log_info("resolution: ", mWidth, "x", mHeight);

    drmModeEncoder *encoder = nullptr;
    if (connector->encoder_id)
    {
        encoder = drmModeGetEncoder(drmDevice, connector->encoder_id);
    }

    if (encoder == nullptr)
    {
        log_err("Unable to get encoder");
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        std::exit(EXIT_FAILURE);
    }

    crtcPtr = drmModeGetCrtc(drmDevice, encoder->crtc_id);
    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);

    gbmDevicePtr = gbm_create_device(drmDevice);
    if (!gbmDevicePtr) {
      log_err("Failed to create GBM device");
      std::exit(EXIT_FAILURE);
    }

    mEgl.eglDisplay = mEgl.eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbmDevicePtr, NULL);
    if (mEgl.eglDisplay == EGL_NO_DISPLAY) {
      log_err("eglGetDisplay failed");
      std::exit(EXIT_FAILURE);
    }

    if (!eglInitialize(mEgl.eglDisplay, nullptr, nullptr)) {
      log_err("eglInitialize failed:", eglGetError());
      std::exit(EXIT_FAILURE);
    }

    EGLint numConfigs = 1;
    if (!eglChooseConfig(mEgl.eglDisplay, mEgl.configAttributes, &mEgl.eglConfig, numConfigs, &numConfigs)) {
      log_err("eglChooseConfig failed:", eglGetError());
      std::exit(EXIT_FAILURE);
    }
    if (numConfigs <= 0) {
      log_err("eglChooseConfig have not found acceptable configurations");
      std::exit(EXIT_FAILURE);
    }
    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_NONE};
    mEgl.eglContext = eglCreateContext(mEgl.eglDisplay, mEgl.eglConfig, EGL_NO_CONTEXT, contextAttributes);
    if (mEgl.eglContext == EGL_NO_CONTEXT) {
      log_err("eglCreateContext failed:", eglGetError());
      std::exit(EXIT_FAILURE);
    }

    gbmSurfacePtr = gbm_surface_create(gbmDevicePtr, mode.hdisplay, mode.vdisplay, GBM_FORMAT_ARGB8888, GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);
    if (!gbmSurfacePtr) {
      log_err("gbm_surface_create failed");
      std::exit(EXIT_FAILURE);
    }

    mEgl.eglSurface = mEgl.eglCreatePlatformWindowSurfaceEXT(mEgl.eglDisplay, mEgl.eglConfig, gbmSurfacePtr, nullptr);
    if (mEgl.eglSurface == EGL_NO_SURFACE) {
      log_err("eglCreateWindowSurface failed:", eglGetError());
      std::exit(EXIT_FAILURE);
    }

    if (!eglMakeCurrent(mEgl.eglDisplay, mEgl.eglSurface, mEgl.eglSurface, mEgl.eglContext)) {
      log_err("eglMakeCurrent failed:", eglGetError());
    }

    mVert = EGL::createShader(vertShader, GL_VERTEX_SHADER);
    mFrag = EGL::createShader(fragShader, GL_FRAGMENT_SHADER);
    mProgram = glCreateProgram();
    if (!mVert || !mFrag || !mProgram) {
      log_err("shader program creation failed");
      std::exit(EXIT_FAILURE);
    }
    glAttachShader(mProgram, mFrag);
    glAttachShader(mProgram, mVert);
    glLinkProgram(mProgram);
    GLint status{0};
    glGetProgramiv(mProgram, GL_LINK_STATUS, &status);
    if (!status) {
      char log[1000];
      GLsizei len;
      glGetProgramInfoLog(mProgram, 1000, &len, log);
      log_err("Error when linking", log);
      std::exit(EXIT_FAILURE);
    }
    mYFlippedUniform = glGetUniformLocation(mProgram, "yFlipped");
    glUniform1i(glGetUniformLocation(mProgram, "inputColor"), 0);
    glBindAttribLocation(mProgram, 0, "position");

    mFBO = std::make_unique<FBO>(mWidth, mHeight);
    if (mFBO && mFBO->fboId) {
      log_info("fbo created");
    } else {
      log_err("No valid FBO!");
      std::exit(EXIT_FAILURE);
    }

    // let's store static data in GPU memory
    glGenBuffers(1, &mVerticesVBO);
    glBindBuffer(GL_ARRAY_BUFFER, mVerticesVBO);
    const GLfloat quadVertices[] = {-1, -1, -1, 1, 1, -1, 1, 1};
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    mWlServer = std::make_unique<WaylandServer>(mEgl, drmDevice);
  }

  void swap() {
    if (mEgl.eglSurface == EGL_NO_SURFACE || mEgl.eglDisplay == EGL_NO_DISPLAY) {
      log_err("EGL backend is not initialized!");
      return;
    }

    if (!gbm_surface_has_free_buffers(gbmSurfacePtr)) {
      log_debug("No available render buffer!");
      return;
    }

    if (eglGetCurrentContext() != mEgl.eglContext) {
      log_err("EGL context lost!");
      return;
    }

    glViewport(0, 0, mWidth, mHeight);
    glClearColor(1, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    // First we draw externals surfaces by itself (under  content)
    if (mWlServer && !WaylandServer::_surfaces.empty()) {
      processExternalSurfaces();
    } else {  // if wayland connection is lost or clients surfaces are gone -> we need to make sure all cached textures
              // are eliminated
      if (!mExternalTexturesAbove.empty() || !mExternalTexturesUnder.empty()) {
        mExternalTexturesAbove.clear();
        mExternalTexturesUnder.clear();
      }
    }
    log_debug("RENDER with resolution:", mWidth, "x", mHeight,
              "surfaces:", mExternalTexturesAbove.size() + mExternalTexturesUnder.size());
    for (const auto &texture : mExternalTexturesUnder) {
      if (!texture) {
        continue;
      }
      drawQuad(texture->textureId, true);
    }

    //glBindFramebuffer(GL_FRAMEBUFFER, mFBO->fboId);
    //flush();
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //drawQuad(mFBO->textureId, false);

    for (const auto &texture : mExternalTexturesAbove) {
      if (!texture) {
        continue;
      }
      drawQuad(texture->textureId, true);
    }

    eglSwapBuffers(mEgl.eglDisplay, mEgl.eglSurface);

    // acquiring the available buffer object
    gbm_bo *bo = gbm_surface_lock_front_buffer(gbmSurfacePtr);
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t pitch = gbm_bo_get_stride(bo);
    uint32_t fb;
    drmModeAddFB(drmDevice, mode.hdisplay, mode.vdisplay, 24, 32, pitch, handle, &fb);
    drmModeSetCrtc(drmDevice, crtcPtr->crtc_id, fb, 0, 0, &connectorId, 1, &mode);

    if (previousBo)
    {
        drmModeRmFB(drmDevice, previousFb);
        gbm_surface_release_buffer(gbmSurfacePtr, previousBo);
    }
    previousBo = bo;
    previousFb = fb;
    
    mWlServer->pollWaylandEvents();
  }

 private:
  bool wakeupScreen() {
    if (mFbDevice < 0) {
      log_err("No valid fb device");
      return false;
    }
    struct fb_var_screeninfo varinfo;
    /* Grab the current screen information. */
    if (ioctl(mFbDevice, FBIOGET_VSCREENINFO, &varinfo) < 0) {
      return false;
    }
    /* force the framebuffer to wake up */
    varinfo.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
    /* Set the device's screen information. */
    if (ioctl(mFbDevice, FBIOPUT_VSCREENINFO, &varinfo) < 0) {
      return false;
    }

    return true;
  }

  void drawQuad(GLuint textureId, bool isYaxisFlipped = false) {
    log_debug("Texture with id", textureId);
    glUseProgram(mProgram);
    glUniform1f(mYFlippedUniform, isYaxisFlipped ? 1.0f : 0.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glBindBuffer(GL_ARRAY_BUFFER, mVerticesVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, mTexCoords);
    glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#ifndef NDEBUG
    auto error = glGetError();
    if (error != GL_NO_ERROR) {
      log_err("GL error:", error);
    }
#endif
  }

  void processExternalSurfaces() {
    static timespec now;
    // if some surface has been updated then we refresh our bundle of textures
    if (!WaylandServer::_isExternalTextureProcessing) {
      mExternalTexturesAbove.clear();
      mExternalTexturesUnder.clear();
      for (auto &surface : WaylandServer::_surfaces) {
        std::pair<bool, WaylandServer::ClientSurface *> &pair = surface.second;
        if (!pair.second) {
          continue;
        }
        // if surface exists and doesn't have corresponding GL texture -> let's generate GL texture
        if (!pair.second->texture) {
          clock_gettime(CLOCK_MONOTONIC, &now);
          auto externalTexture =
              std::make_shared<WaylandServer::Texture>(mEgl, pair.second->width, pair.second->height);

          if (pair.second->dmabuf.width > 0 && pair.second->dmabuf.height > 0) {
            log_debug("dma buf:", pair.second->dmabuf.width, "x", pair.second->dmabuf.height);

            EGLint attribs[] = {EGL_WIDTH,
                                pair.second->dmabuf.width,
                                EGL_HEIGHT,
                                pair.second->dmabuf.height,
                                EGL_LINUX_DRM_FOURCC_EXT,
                                static_cast<EGLint>(pair.second->dmabuf.format),
                                EGL_DMA_BUF_PLANE0_FD_EXT,
                                pair.second->dmabuf.fd[0],
                                EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                                static_cast<EGLint>(pair.second->dmabuf.offset[0]),
                                EGL_DMA_BUF_PLANE0_PITCH_EXT,
                                static_cast<EGLint>(pair.second->dmabuf.stride[0]),
                                EGL_NONE};
            externalTexture->image =
                mEgl.eglCreateImageKHR(mEgl.eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs);
            if (externalTexture->image == EGL_NO_IMAGE_KHR) {
              log_err("Couldn't create dma based image ");
              externalTexture = nullptr;
            } else {
              mEgl.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, externalTexture->image);
              // check for gl error (DEBUG build only)
#ifndef NDEBUG
              auto error = glGetError();
              if (error != GL_NO_ERROR) {
                log_err("GL error:", error);
                externalTexture = nullptr;
              }
#endif
            }
          } else if (pair.second->sharedMem) {
            log_debug("shmem buf:", pair.second->width, "x", pair.second->height);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, pair.second->width, pair.second->height, 0, GL_BGRA_EXT,
                         GL_UNSIGNED_BYTE, pair.second->sharedMem);
          } else {
            log_err("No valid buffer source!");
            externalTexture = nullptr;
          }
          pair.second->texture = externalTexture;
          mWlServer->sendFrameDoneForClient(static_cast<wlr_surface *>(surface.first), &now);
        }
        // if corresponding OpenGL texture exists
        if (pair.second->texture) {
          // is the surface drawn above 
          if (pair.first) {
            mExternalTexturesAbove.push_back(pair.second->texture);
            log_debug("topmost external texture added!");
          } else {
            mExternalTexturesUnder.push_back(pair.second->texture);
            log_debug("regular external texture added!");
          }
        }
      }
      WaylandServer::_isExternalTextureProcessing = true;
    }
  }

  void initFbdev() {
    // clear fb0 before drawing to avoid noise showing
    system("cat /dev/zero > /dev/fb0");

    mFbDevice = open("/dev/fb0", O_RDWR);
    if (mFbDevice < 0) {
      log_err("Failed to open fbdev device");
      std::exit(EXIT_FAILURE);
    }
    struct fb_var_screeninfo vinfo;
    memset(&vinfo, 0, sizeof(vinfo));
    if (ioctl(mFbDevice, FBIOGET_VSCREENINFO, &vinfo)) {
      log_err("Failed to get fb info");
    }

    log_info("making display active:", wakeupScreen());

    int fbWidth = vinfo.xres;
    int fbHeight = vinfo.yres;
    int fbBpp = vinfo.bits_per_pixel;

    mWidth = fbWidth;
    mHeight = fbHeight;

    log_info("fb info:", fbWidth, "x", fbHeight, "bpp:", fbBpp);
  }

  void cleanUp() {
    if (mEgl.eglDisplay != EGL_NO_DISPLAY) {
      glDeleteBuffers(1, &mVerticesVBO);
      glDeleteShader(mVert);
      glDeleteShader(mFrag);
      glDeleteProgram(mProgram);
      eglMakeCurrent(mEgl.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (mEgl.eglContext != EGL_NO_CONTEXT) {
        eglDestroyContext(mEgl.eglDisplay, mEgl.eglContext);
      }
      if (mEgl.eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mEgl.eglDisplay, mEgl.eglSurface);
      }
      eglReleaseThread();
      eglTerminate(mEgl.eglDisplay);
    }
    if (mFbDevice >= 0) {
      close(mFbDevice);
    }

    drmModeFreeCrtc(crtcPtr);

    if (previousBo)
    {
        drmModeRmFB(drmDevice, previousFb);
        gbm_surface_release_buffer(gbmSurfacePtr, previousBo);
    }
    gbm_surface_destroy(gbmSurfacePtr);
    gbm_device_destroy(gbmDevicePtr);

    close(drmDevice);
  }
};

int main()
{
  int screenWidth = 1920;
  int screenHeight = 1080;

  std::unique_ptr<DrmWindow> fbDev = std::make_unique<DrmWindow>(screenWidth, screenHeight);
  fbDev->initialize();
  while (true) {
    fbDev->swap();
    usleep(200);
  }

  return 0;
}