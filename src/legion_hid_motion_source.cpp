#include "legion_hid_motion_source.hpp"

#include "legion_hid_protocol.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <linux/hidraw.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace motion
{
namespace
{
constexpr std::uint16_t LenovoVendorId = 0x17EF;
constexpr int ReadTimeoutMilliseconds = 1000;
constexpr int ReconnectAttempts = 3;
constexpr auto ReconnectDelay = std::chrono::milliseconds(500);
constexpr std::uint32_t MissingControllerReportLimit = 125;

bool is_legion_vendor_interface(int fd)
{
    hidraw_report_descriptor descriptor{};
    int descriptor_size = 0;
    if (ioctl(fd, HIDIOCGRDESCSIZE, &descriptor_size) < 0 ||
        descriptor_size <= 0 || descriptor_size > HID_MAX_DESCRIPTOR_SIZE)
        return false;

    descriptor.size = static_cast<__u32>(descriptor_size);
    if (ioctl(fd, HIDIOCGRDESC, &descriptor) < 0)
        return false;

    // Public protocol documentation identifies the raw interface by vendor
    // usage page 0xFFA0 and usage 0x0001. Match their short HID encodings.
    const std::array<std::uint8_t, 3> usage_page = {0x06, 0xA0, 0xFF};
    const std::array<std::uint8_t, 2> usage = {0x09, 0x01};
    const auto begin = descriptor.value;
    const auto end = descriptor.value + descriptor.size;
    return std::search(begin, end, usage_page.begin(), usage_page.end()) != end &&
           std::search(begin, end, usage.begin(), usage.end()) != end;
}

auto hidraw_paths() -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> paths;
    std::error_code error;
    for (const auto& entry :
         std::filesystem::directory_iterator("/dev", error))
    {
        const auto name = entry.path().filename().string();
        if (name.rfind("hidraw", 0) == 0)
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}
} // namespace

LegionHIDMotionSource::~LegionHIDMotionSource()
{
    close_device();
}

void LegionHIDMotionSource::close_device()
{
    if (fd_ >= 0)
    {
        for (const auto& command :
             legion_protocol::shutdown_commands(selected_side_))
            static_cast<void>(write(fd_, command.data(), command.size()));
        close(fd_);
    }
    fd_ = -1;
    have_timestamp_ = false;
    have_valid_gyro_ = false;
    missing_controller_reports_ = 0;
}

bool LegionHIDMotionSource::initialize()
{
    close_device();

    for (const auto& path : hidraw_paths())
    {
        const int candidate = open(path.c_str(), O_RDWR | O_CLOEXEC);
        if (candidate < 0)
            continue;

        hidraw_devinfo info{};
        if (ioctl(candidate, HIDIOCGRAWINFO, &info) == 0 &&
            info.vendor == LenovoVendorId &&
            legion_protocol::is_supported_product(info.product) &&
            is_legion_vendor_interface(candidate))
        {
            fd_ = candidate;
            if (send_initialization_packets())
            {
                std::cout << "Using Legion controller HID interface "
                          << path << '\n';
                return true;
            }
            fd_ = -1;
        }
        close(candidate);
    }

    std::cerr << "No usable Lenovo Legion controller HID interface found\n";
    return false;
}

bool LegionHIDMotionSource::reconnect()
{
    close_device();
    for (int attempt = 1; attempt <= ReconnectAttempts; ++attempt)
    {
        std::cerr << "Reconnecting Legion HID motion source (attempt "
                  << attempt << '/' << ReconnectAttempts << ")\n";
        if (initialize())
            return true;
        if (attempt != ReconnectAttempts)
            std::this_thread::sleep_for(ReconnectDelay);
    }
    return false;
}

bool LegionHIDMotionSource::send_initialization_packets()
{
    for (const auto& command :
         legion_protocol::initialization_commands(selected_side_))
    {
        const auto written = write(fd_, command.data(), command.size());
        if (written != static_cast<ssize_t>(command.size()))
        {
            std::cerr << "Unable to initialize Legion controller IMU: "
                      << std::strerror(errno) << '\n';
            return false;
        }
    }
    return true;
}

bool LegionHIDMotionSource::switch_selected_side(
    legion_protocol::ControllerSide side)
{
    for (const auto& command :
         legion_protocol::shutdown_commands(selected_side_))
        static_cast<void>(write(fd_, command.data(), command.size()));

    selected_side_ = side;
    have_timestamp_ = false;
    have_valid_gyro_ = false;
    if (!send_initialization_packets())
        return false;

    std::cout << "Using "
              << (selected_side_ == legion_protocol::ControllerSide::Right
                      ? "right"
                      : "left")
              << " Legion controller IMU\n";
    return true;
}

bool LegionHIDMotionSource::poll(MotionSample& sample)
{
    while (fd_ >= 0)
    {
        pollfd descriptor{fd_, POLLIN, 0};
        const int ready = ::poll(&descriptor, 1, ReadTimeoutMilliseconds);
        if (ready < 0 && errno == EINTR)
            return false;
        if (ready <= 0 || (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)))
        {
            if (ready == 0)
                std::cerr << "Timed out waiting for Legion HID motion report\n";
            else
                std::cerr << "Legion HID device became unavailable\n";
            if (reconnect())
                continue;
            return false;
        }

        legion_protocol::Report report{};
        const auto length = read(fd_, report.data(), report.size());
        if (length < 0 && errno == EINTR)
            continue;
        if (length != static_cast<ssize_t>(report.size()))
        {
            std::cerr << "Invalid Legion HID report length: " << length << '\n';
            if (reconnect())
                continue;
            return false;
        }

        const auto connected_side =
            legion_protocol::connected_controller_side(report);
        if (!connected_side)
        {
            ++missing_controller_reports_;
            if (missing_controller_reports_ < MissingControllerReportLimit)
                continue;
            std::cerr << "No connected Legion controller reported by HID\n";
            if (reconnect())
                continue;
            return false;
        }
        missing_controller_reports_ = 0;

        if (*connected_side != selected_side_)
        {
            if (!switch_selected_side(*connected_side) && !reconnect())
                return false;
            continue;
        }

        std::uint8_t timestamp = 0;
        // A DSU slot carries one IMU. Only the selected controller is enabled.
        if (!legion_protocol::decode_report(report, selected_side_, sample,
                                            timestamp))
            continue;

        if (legion_protocol::has_known_gyro_glitch(report, selected_side_))
        {
            ++gyro_glitch_count_;
            if (gyro_glitch_count_ == 1 || gyro_glitch_count_ % 100 == 0)
                std::cerr << "Discarded known Legion controller gyro glitch ("
                          << gyro_glitch_count_ << " total)\n";
            if (!have_valid_gyro_)
                continue;
            sample.gyro = last_valid_gyro_;
        } else
        {
            last_valid_gyro_ = sample.gyro;
            have_valid_gyro_ = true;
        }

        const auto host_now = std::chrono::steady_clock::now();
        if (have_timestamp_)
        {
            const auto device_delta =
                legion_protocol::timestamp_delta_seconds(previous_timestamp_,
                                                          timestamp);
            const auto host_delta =
                std::chrono::duration<double>(host_now - previous_host_time_)
                    .count();
            if (legion_protocol::is_plausible_timestamp_delta(device_delta,
                                                              host_delta))
            {
                sample.dt_seconds = device_delta;
            } else
            {
                sample.dt_seconds = host_delta;
                ++timestamp_fallback_count_;
                if (timestamp_fallback_count_ == 1 ||
                    timestamp_fallback_count_ % 100 == 0)
                    std::cerr << "Using host timing for implausible Legion HID "
                                 "timestamp ("
                              << timestamp_fallback_count_ << " total)\n";
            }
        } else
        {
            sample.dt_seconds = 0.0;
        }
        previous_timestamp_ = timestamp;
        previous_host_time_ = host_now;
        have_timestamp_ = true;
        return true;
    }
    return false;
}
} // namespace motion
