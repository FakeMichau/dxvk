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

#undef LOAD_PFN
  }

  Lfx2Fn::~Lfx2Fn() {
    if (m_lfxModule == nullptr)
      return;

    ::FreeLibrary(m_lfxModule);
    m_lfxModule = nullptr;
  }

  template<typename T>
  T Lfx2Fn::GetProcAddress(const char *name) {
    return reinterpret_cast<T>(reinterpret_cast<void *>(::GetProcAddress(m_lfxModule, name)));
  }

  DxvkLfx2Tracker::DxvkLfx2Tracker(DxvkDevice *device) : m_device(device) {
  }

  void DxvkLfx2Tracker::add(Lfx2Frame lfx2Frame, Rc<DxvkGpuQuery> query, bool end) {
    m_query[end] = std::move(query);
    m_frame_handle[end] = std::move(lfx2Frame);
  }

  void DxvkLfx2Tracker::notify() {
    for (uint32_t i = 0; i < 2; i++) {
      Rc<DxvkGpuQuery> &query = m_query[i];
      if (query.ptr()) {
        DxvkQueryData queryData; // NOLINT(cppcoreguidelines-pro-type-member-init)
        DxvkGpuQueryStatus status;
        while ((status = query->getData(queryData)) == DxvkGpuQueryStatus::Pending);

        if (status == DxvkGpuQueryStatus::Available) {
          uint64_t gpuTimestamp = queryData.timestamp.time;
          VkCalibratedTimestampInfoEXT calibratedTimestampInfo[2];
          uint64_t calibratedTimestamps[2];
          uint64_t maxDeviation[2];
          calibratedTimestampInfo[0].sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT;
          calibratedTimestampInfo[0].pNext = nullptr;
          calibratedTimestampInfo[0].timeDomain = VK_TIME_DOMAIN_DEVICE_EXT;
          calibratedTimestampInfo[1].sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT;
          calibratedTimestampInfo[1].pNext = nullptr;
#ifdef _WIN32
          calibratedTimestampInfo[1].timeDomain = VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT;
#else
          calibratedTimestampInfo[1].timeDomain = VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT;
#endif
          m_device->vkd()->vkGetCalibratedTimestampsEXT(m_device->handle(), 2, calibratedTimestampInfo,
                                                        calibratedTimestamps, maxDeviation);

#ifdef _WIN32
          uint64_t hostNsTimestamp = m_device->lfx2().TimestampFromQpc(calibratedTimestamps[1]);
#else
          uint64_t hostNsTimestamp = calibratedTimestamps[1];
#endif
          int64_t gpuTimestampDelta = gpuTimestamp - calibratedTimestamps[0];
          int64_t timestamp = hostNsTimestamp + (int64_t) (gpuTimestampDelta *
                                                           (double) m_device->adapter()->deviceProperties().limits.timestampPeriod);

          m_device->lfx2().MarkSection(m_frame_handle[i],
                                       1000, i == 0 ? lfx2MarkType::lfx2MarkTypeBegin : lfx2MarkType::lfx2MarkTypeEnd,
                                       timestamp);
        }
      }
    }
  }

  void DxvkLfx2Tracker::reset() {
    for (auto &i: m_query) {
      i = nullptr;
    }
    for (auto &i: m_frame_handle) {
      i = {};
    }
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