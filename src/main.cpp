#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_wgpu.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <emscripten.h>
#include <functional>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static std::function<void()> MainLoopForEmscriptenP;
static void MainLoopForEmscripten() { MainLoopForEmscriptenP(); }
#define EMSCRIPTEN_MAINLOOP_BEGIN MainLoopForEmscriptenP = [&]() { do
#define EMSCRIPTEN_MAINLOOP_END                                                \
  while (0)                                                                    \
    ;                                                                          \
  }                                                                            \
  ;                                                                            \
  emscripten_set_main_loop(MainLoopForEmscripten, 0, true)

static WGPUDevice wgpu_device = nullptr;
static WGPUSurface wgpu_surface = nullptr;
static WGPUQueue wgpu_queue = nullptr;
static WGPUSurfaceConfiguration wgpu_config = {};
static int surface_w = 1280, surface_h = 800;

static WGPUTextureView img_view = nullptr;
static int img_w = 0, img_h = 0;

static void ConfigureSurface() {
  wgpu_config.width = surface_w;
  wgpu_config.height = surface_h;
  wgpuSurfaceConfigure(wgpu_surface, &wgpu_config);
}

static bool LoadImage(const char *path) {
  int ch;
  unsigned char *pixels = stbi_load(path, &img_w, &img_h, &ch, 4);
  if (!pixels) {
    fprintf(stderr, "Failed to load %s\n", path);
    return false;
  }

  WGPUTextureDescriptor td = {};
  td.dimension = WGPUTextureDimension_2D;
  td.size = {(uint32_t)img_w, (uint32_t)img_h, 1};
  td.format = WGPUTextureFormat_RGBA8Unorm;
  td.mipLevelCount = 1;
  td.sampleCount = 1;
  td.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding;
  WGPUTexture tex = wgpuDeviceCreateTexture(wgpu_device, &td);

  WGPUTexelCopyTextureInfo dst = {};
  dst.texture = tex;
  dst.aspect = WGPUTextureAspect_All;

  WGPUTexelCopyBufferLayout layout = {};
  layout.bytesPerRow = (uint32_t)(img_w * 4);
  layout.rowsPerImage = (uint32_t)img_h;

  WGPUExtent3D size = {(uint32_t)img_w, (uint32_t)img_h, 1};
  wgpuQueueWriteTexture(wgpu_queue, &dst, pixels, (size_t)(img_w * img_h * 4),
                        &layout, &size);
  stbi_image_free(pixels);

  WGPUTextureViewDescriptor vd = {};
  vd.format = WGPUTextureFormat_RGBA8Unorm;
  vd.dimension = WGPUTextureViewDimension_2D;
  vd.mipLevelCount = 1;
  vd.arrayLayerCount = 1;
  vd.aspect = WGPUTextureAspect_All;
  img_view = wgpuTextureCreateView(tex, &vd);
  return true;
}

static bool InitWGPU() {
  wgpu::InstanceDescriptor idesc = {};
  idesc.capabilities.timedWaitAnyEnable = true;
  wgpu::Instance instance = wgpu::CreateInstance(&idesc);

  wgpu::Adapter adapter;
  wgpu::RequestAdapterOptions ao;
  wgpu::Future f1 = instance.RequestAdapter(
      &ao, wgpu::CallbackMode::WaitAnyOnly,
      [&](wgpu::RequestAdapterStatus s, wgpu::Adapter a, wgpu::StringView) {
        if (s == wgpu::RequestAdapterStatus::Success)
          adapter = std::move(a);
      });
  instance.WaitAny(f1, UINT64_MAX);

  wgpu::DeviceDescriptor dd;
  dd.SetUncapturedErrorCallback(
      [](const wgpu::Device &, wgpu::ErrorType t, wgpu::StringView m) {
        fprintf(stderr, "WebGPU error %d: %s\n", (int)t, m.data);
      });
  wgpu::Device device;
  wgpu::Future f2 = adapter.RequestDevice(
      &dd, wgpu::CallbackMode::WaitAnyOnly,
      [&](wgpu::RequestDeviceStatus s, wgpu::Device d, wgpu::StringView) {
        if (s == wgpu::RequestDeviceStatus::Success)
          device = std::move(d);
      });
  instance.WaitAny(f2, UINT64_MAX);
  wgpu_device = device.MoveToCHandle();

  wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvas = {};
  canvas.selector = "#canvas";
  wgpu::SurfaceDescriptor sd = {};
  sd.nextInChain = &canvas;
  wgpu_surface = instance.CreateSurface(&sd).MoveToCHandle();
  if (!wgpu_surface)
    return false;

  WGPUSurfaceCapabilities caps = {};
  wgpuSurfaceGetCapabilities(wgpu_surface, adapter.Get(), &caps);

  wgpu_config.presentMode = WGPUPresentMode_Fifo;
  wgpu_config.alphaMode = WGPUCompositeAlphaMode_Auto;
  wgpu_config.usage = WGPUTextureUsage_RenderAttachment;
  wgpu_config.device = wgpu_device;
  wgpu_config.format = caps.formats[0];
  wgpu_config.width = surface_w;
  wgpu_config.height = surface_h;
  ConfigureSurface();

  wgpu_queue = wgpuDeviceGetQueue(wgpu_device);
  wgpuInstanceRelease(instance.MoveToCHandle());
  return true;
}

int main(int, char **) {
  glfwSetErrorCallback(
      [](int e, const char *d) { fprintf(stderr, "GLFW %d: %s\n", e, d); });
  if (!glfwInit())
    return 1;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  GLFWwindow *window = glfwCreateWindow(surface_w, surface_h,
                                        "ImGui WebGPU Repro", nullptr, nullptr);
  if (!window || !InitWGPU())
    return 1;
  glfwShowWindow(window);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOther(window, true);
  ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");

  ImGui_ImplWGPU_InitInfo wi;
  wi.Device = wgpu_device;
  wi.NumFramesInFlight = 3;
  wi.RenderTargetFormat = wgpu_config.format;
  wi.DepthStencilFormat = WGPUTextureFormat_Undefined;
  ImGui_ImplWGPU_Init(&wi);

  LoadImage("/Resources/test.png");

  ImVec4 clear = {0.45f, 0.55f, 0.60f, 1.00f};

  EMSCRIPTEN_MAINLOOP_BEGIN {
    glfwPollEvents();

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    if (w != surface_w || h != surface_h) {
      surface_w = w;
      surface_h = h;
      ConfigureSurface();
    }

    WGPUSurfaceTexture st;
    wgpuSurfaceGetCurrentTexture(wgpu_surface, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
      if (st.texture)
        wgpuTextureRelease(st.texture);
      ConfigureSurface();
      continue;
    }

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    if (img_view) {
      ImGui::Begin("Image");
      ImVec2 avail = ImGui::GetContentRegionAvail();
      float scale =
          (avail.x > 0 && img_w > 0) ? fminf(avail.x / img_w, 1.0f) : 1.0f;
      ImGui::Image((ImTextureID)img_view, ImVec2(img_w * scale, img_h * scale));
      ImGui::End();
    }

    ImGui::Render();

    WGPUTextureViewDescriptor tvd = {};
    tvd.format = wgpu_config.format;
    tvd.dimension = WGPUTextureViewDimension_2D;
    tvd.mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED;
    tvd.arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED;
    tvd.aspect = WGPUTextureAspect_All;
    WGPUTextureView tv = wgpuTextureCreateView(st.texture, &tvd);

    WGPURenderPassColorAttachment ca = {};
    ca.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    ca.loadOp = WGPULoadOp_Clear;
    ca.storeOp = WGPUStoreOp_Store;
    ca.clearValue = {clear.x * clear.w, clear.y * clear.w, clear.z * clear.w,
                     clear.w};
    ca.view = tv;

    WGPURenderPassDescriptor rpd = {};
    rpd.colorAttachmentCount = 1;
    rpd.colorAttachments = &ca;

    WGPUCommandEncoder enc =
        wgpuDeviceCreateCommandEncoder(wgpu_device, nullptr);
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rpd);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
    wgpuRenderPassEncoderEnd(pass);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(wgpu_queue, 1, &cmd);

    wgpuTextureViewRelease(tv);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(enc);
    wgpuCommandBufferRelease(cmd);
  }
  EMSCRIPTEN_MAINLOOP_END;

  ImGui_ImplWGPU_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
