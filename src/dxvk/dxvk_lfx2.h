#pragma once

#include "dxvk_gpu_query.h"
#include "latencyflex2.h"

namespace dxvk {

  class DxvkLfx2 {
  public:
    DxvkLfx2();
    virtual ~DxvkLfx2();

#define DECLARE_PFN(x) \
    decltype(&::lfx2##x) x {}

    DECLARE_PFN(ContextCreate);
    DECLARE_PFN(ContextAddRef);
    DECLARE_PFN(ContextRelease);
    DECLARE_PFN(FrameCreate);
    DECLARE_PFN(FrameAddRef);
    DECLARE_PFN(FrameRelease);
    DECLARE_PFN(MarkSection);
    DECLARE_PFN(SleepUntil);
    DECLARE_PFN(TimestampNow);
#ifdef _WIN32
    DECLARE_PFN(TimestampFromQpc);
#endif

#undef DECLARE_PFN

  private:
    template <typename T>
    T GetProcAddress(const char* name);

    HMODULE m_lfxModule{};
  };

  class DxvkLfx2Tracker {
  public:
    explicit DxvkLfx2Tracker(DxvkDevice *device);
    void add(void *lfx2Frame, Rc<DxvkGpuQuery> query, bool end);
    void reset();
    void notify();

  private:
    DxvkDevice *m_device;
    Rc<DxvkGpuQuery> m_query[2]{};
    void *m_frame_handle[2]{};
  };

} // dxvk