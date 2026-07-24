#pragma once

#include "legion_hid_protocol.hpp"
#include "motion_source.hpp"

#include <chrono>
#include <cstdint>

namespace motion
{
class LegionHIDMotionSource final : public MotionSource
{
  public:
    LegionHIDMotionSource() = default;
    ~LegionHIDMotionSource() override;

    LegionHIDMotionSource(const LegionHIDMotionSource&) = delete;
    auto operator=(const LegionHIDMotionSource&)
        -> LegionHIDMotionSource& = delete;

    bool initialize() override;
    bool poll(MotionSample& sample) override;

  private:
    void close_device();
    bool reconnect();
    bool send_initialization_packets();
    bool switch_selected_side(legion_protocol::ControllerSide side);

    int fd_ = -1;
    bool have_timestamp_ = false;
    std::uint8_t previous_timestamp_ = 0;
    std::chrono::steady_clock::time_point previous_host_time_;
    std::uint64_t timestamp_fallback_count_ = 0;
    bool have_valid_gyro_ = false;
    Vec3 last_valid_gyro_;
    std::uint64_t gyro_glitch_count_ = 0;
    std::uint32_t missing_controller_reports_ = 0;
    legion_protocol::ControllerSide selected_side_ =
        legion_protocol::ControllerSide::Right;
};
} // namespace motion
