#pragma once

#include <deque>
#include "dxvk_gpu_query.h"
#include "latencyflex2.h"

namespace dxvk {

  class Lfx2Fn {
  public:
    Lfx2Fn();
    virtual ~Lfx2Fn();

#define DECLARE_PFN(x) \
    decltype(&::lfx2##x) x {}

    DECLARE_PFN(ContextCreate);
    DECLARE_PFN(ContextAddRef);
    DECLARE_PFN(ContextRelease);
    DECLARE_PFN(FrameCreate);
    DECLARE_PFN(FrameAddRef);
    DECLARE_PFN(FrameRelease);
    DECLARE_PFN(MarkSection);
    DECLARE_PFN(FrameOverrideQueuingDelay);
    DECLARE_PFN(FrameOverrideInverseThroughput);
    DECLARE_PFN(SleepUntil);
    DECLARE_PFN(TimestampNow);
#ifdef _WIN32
    DECLARE_PFN(TimestampFromQpc);
#endif
    DECLARE_PFN(ImplicitContextCreate);
    DECLARE_PFN(ImplicitContextRelease);
    DECLARE_PFN(ImplicitContextReset);
    DECLARE_PFN(FrameCreateImplicit);
    DECLARE_PFN(FrameDequeueImplicit);
    DECLARE_PFN(VulkanContextCreate);
    DECLARE_PFN(VulkanContextAddRef);
    DECLARE_PFN(VulkanContextRelease);
    DECLARE_PFN(VulkanContextBeforeSubmit);
    DECLARE_PFN(VulkanContextBeginFrame);
    DECLARE_PFN(VulkanContextEndFrame);

#undef DECLARE_PFN

  private:
    template <typename T>
    T GetProcAddress(const char* name);

    HMODULE m_lfxModule{};
  };

  class Lfx2Frame {
  public:
    Lfx2Frame();
    Lfx2Frame(const Lfx2Fn &lfx2, lfx2Frame *lfx2Frame);
    Lfx2Frame(const Lfx2Frame &other);
    Lfx2Frame(Lfx2Frame &&other) noexcept;
    ~Lfx2Frame();

    Lfx2Frame& operator=(const Lfx2Frame &other);
    Lfx2Frame& operator=(Lfx2Frame &&other) noexcept;

    operator lfx2Frame *() const { return m_lfx2Frame; }

  private:
    const Lfx2Fn *m_lfx2{};
    lfx2Frame *m_lfx2Frame{};
  };

  class DxvkLfx2ImplicitContext {
  public:
    explicit DxvkLfx2ImplicitContext(Lfx2Fn *lfx2);
    ~DxvkLfx2ImplicitContext();
    lfx2ImplicitContext *context() const { return m_context; }
    Lfx2Frame dequeueFrame(bool critical);
    void reset();

  private:
    Lfx2Fn *m_lfx2;
    lfx2ImplicitContext *m_context;
  };

} // dxvk