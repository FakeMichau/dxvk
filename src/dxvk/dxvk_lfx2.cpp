#include "dxvk_lfx2.h"

#include "../util/util_time.h"
#include "dxvk_device.h"
#include "../util/util_win32_compat.h"

namespace dxvk {

  DxvkLfx2::DxvkLfx2() {
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

#undef LOAD_PFN
  }

  DxvkLfx2::~DxvkLfx2() {
    if (m_lfxModule == nullptr)
      return;

    ::FreeLibrary(m_lfxModule);
    m_lfxModule = nullptr;
  }

  template<typename T>
  T DxvkLfx2::GetProcAddress(const char *name) {
    return reinterpret_cast<T>(reinterpret_cast<void *>(::GetProcAddress(m_lfxModule, name)));
  }

  DxvkLfx2Tracker::DxvkLfx2Tracker(DxvkDevice *device) : m_device(device) {
  }

  void DxvkLfx2Tracker::add(void *lfx2Frame, Rc<DxvkGpuQuery> query, bool end) {
    m_query[end] = std::move(query);
    m_frame_handle[end] = lfx2Frame;
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
          calibratedTimestampInfo[1].timeDomain = VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT;
          m_device->vkd()->vkGetCalibratedTimestampsEXT(m_device->handle(), 2, calibratedTimestampInfo,
                                                        calibratedTimestamps, maxDeviation);

          uint64_t hostNsTimestamp = dxvk::high_resolution_clock::to_ns(calibratedTimestamps[1]);
          int64_t gpuTimestampDelta = gpuTimestamp - calibratedTimestamps[0];
          int64_t timestamp = hostNsTimestamp + (int64_t) (gpuTimestampDelta *
                                                           (double) m_device->adapter()->deviceProperties().limits.timestampPeriod);

          m_device->lfx2().MarkSection(static_cast<const lfx2Frame *>(m_frame_handle[i]),
                                       1000, i == 0 ? lfx2MarkType::lfx2MarkTypeBegin : lfx2MarkType::lfx2MarkTypeEnd,
                                       timestamp);
          m_device->lfx2().FrameRelease(static_cast<const lfx2Frame *>(m_frame_handle[i]));
        }
      }
    }
  }

  void DxvkLfx2Tracker::reset() {
    for (auto &i: m_query) {
      i = nullptr;
    }
    for (auto &i: m_frame_handle) {
      i = nullptr;
    }
  }

} // dxvk