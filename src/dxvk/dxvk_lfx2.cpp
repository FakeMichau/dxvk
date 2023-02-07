#include "dxvk_lfx2.h"

#include <utility>

#include "../util/util_time.h"
#include "dxvk_device.h"
#include "../util/util_win32_compat.h"

namespace dxvk {

  Lfx2Fn::Lfx2Fn() {
#ifdef _WIN32
    const auto lfxModuleName = "latencyflex2_rust.dll";
#else
    const auto lfxModuleName = "liblatencyflex2_rust.so";
#endif

    m_lfxModule = ::LoadLibraryA(lfxModuleName);
    if (m_lfxModule == nullptr) {
      Logger::err(str::format("Failed to load ", lfxModuleName));
      return;
    }

#define LOAD_PFN(x) \
    this->x = GetProcAddress<decltype(&::lfx2##x)>("lfx2" #x)

    LOAD_PFN(ContextCreate);
    LOAD_PFN(ContextAddRef);
    LOAD_PFN(ContextRelease);
    LOAD_PFN(FrameCreate);
    LOAD_PFN(FrameAddRef);
    LOAD_PFN(FrameRelease);
    LOAD_PFN(MarkSection);
    LOAD_PFN(FrameOverrideQueuingDelay);
    LOAD_PFN(FrameOverrideInverseThroughput);
    LOAD_PFN(SleepUntil);
    LOAD_PFN(TimestampNow);
#ifdef _WIN32
    LOAD_PFN(TimestampFromQpc);
#endif
    LOAD_PFN(ImplicitContextCreate);
    LOAD_PFN(ImplicitContextRelease);
    LOAD_PFN(ImplicitContextReset);
    LOAD_PFN(FrameCreateImplicit);
    LOAD_PFN(FrameDequeueImplicit);
    LOAD_PFN(VulkanContextCreate);
    LOAD_PFN(VulkanContextAddRef);
    LOAD_PFN(VulkanContextRelease);
    LOAD_PFN(VulkanContextBeforeSubmit);
    LOAD_PFN(VulkanContextBeginFrame);
    LOAD_PFN(VulkanContextEndFrame);

#undef LOAD_PFN
  }

  Lfx2Fn::~Lfx2Fn() {
    if (m_lfxModule == nullptr)
      return;

    // Calling FreeLibrary deadlocks if called from DllMain.
    if (this_thread::isInModuleDetachment())
      return;

    ::FreeLibrary(m_lfxModule);
    m_lfxModule = nullptr;
  }

  template<typename T>
  T Lfx2Fn::GetProcAddress(const char *name) {
    return reinterpret_cast<T>(reinterpret_cast<void *>(::GetProcAddress(m_lfxModule, name)));
  }

  DxvkLfx2ImplicitContext::DxvkLfx2ImplicitContext(Lfx2Fn *lfx2): m_lfx2(lfx2) {
    m_context = m_lfx2->ImplicitContextCreate();
  }

  DxvkLfx2ImplicitContext::~DxvkLfx2ImplicitContext() {
    m_lfx2->ImplicitContextRelease(m_context);
  }

  Lfx2Frame DxvkLfx2ImplicitContext::dequeueFrame(bool critical) {
    lfx2Frame *frame = m_lfx2->FrameDequeueImplicit(m_context, critical);
    Lfx2Frame wrapper(*m_lfx2, frame);
    if (frame)
      m_lfx2->FrameRelease(frame);
    return wrapper;
  }

  void DxvkLfx2ImplicitContext::reset() {
    m_lfx2->ImplicitContextReset(m_context);
  }

  Lfx2Frame::Lfx2Frame() {

  }

  Lfx2Frame::Lfx2Frame(const Lfx2Fn &lfx2, lfx2Frame *lfx2Frame) : m_lfx2(&lfx2), m_lfx2Frame(lfx2Frame) {
    if (m_lfx2Frame)
      m_lfx2->FrameAddRef(m_lfx2Frame);
  }

  Lfx2Frame::~Lfx2Frame() {
    if (m_lfx2Frame != nullptr)
      m_lfx2->FrameRelease(m_lfx2Frame);
  }

  Lfx2Frame::Lfx2Frame(const Lfx2Frame &other): m_lfx2(other.m_lfx2), m_lfx2Frame(other.m_lfx2Frame) {
    m_lfx2->FrameAddRef(m_lfx2Frame);
  }

  Lfx2Frame::Lfx2Frame(Lfx2Frame &&other) noexcept : m_lfx2(other.m_lfx2), m_lfx2Frame(other.m_lfx2Frame) {
    other.m_lfx2Frame = nullptr;
  }

  Lfx2Frame &Lfx2Frame::operator=(const Lfx2Frame &other) {
    if (this != &other) {
      if (m_lfx2Frame != nullptr)
        m_lfx2->FrameRelease(m_lfx2Frame);

      m_lfx2 = other.m_lfx2;
      m_lfx2Frame = other.m_lfx2Frame;
      m_lfx2->FrameAddRef(m_lfx2Frame);
    }

    return *this;
  }

  Lfx2Frame &Lfx2Frame::operator=(Lfx2Frame &&other) noexcept {
    if (m_lfx2Frame != nullptr)
      m_lfx2->FrameRelease(m_lfx2Frame);

    m_lfx2 = other.m_lfx2;
    m_lfx2Frame = other.m_lfx2Frame;
    other.m_lfx2Frame = nullptr;
    return *this;
  }
} // dxvk