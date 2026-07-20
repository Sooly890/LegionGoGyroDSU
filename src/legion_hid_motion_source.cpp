#include "legion_hid_motion_source.hpp"

#include "legion_hid_protocol.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <linux/hidraw.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

namespace motion
{
namespace
{
constexpr std::uint16_t LenovoVendorId = 0x17EF;
constexpr int ReadTimeoutMilliseconds = 1000;

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
    if (fd_ >= 0)
        close(fd_);
}

bool LegionHIDMotionSource::initialize()
{
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

bool LegionHIDMotionSource::send_initialization_packets()
{
    for (const auto& command : legion_protocol::initialization_commands())
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

bool LegionHIDMotionSource::poll(MotionSample& sample)
{
    while (fd_ >= 0)
    {
        pollfd descriptor{fd_, POLLIN, 0};
        const int ready = ::poll(&descriptor, 1, ReadTimeoutMilliseconds);
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready <= 0 || (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)))
        {
            if (ready == 0)
                std::cerr << "Timed out waiting for Legion HID motion report\n";
            else
                std::cerr << "Legion HID device became unavailable\n";
            return false;
        }

        legion_protocol::Report report{};
        const auto length = read(fd_, report.data(), report.size());
        if (length < 0 && errno == EINTR)
            continue;
        if (length != static_cast<ssize_t>(report.size()))
        {
            std::cerr << "Invalid Legion HID report length: " << length << '\n';
            return false;
        }

        std::uint8_t timestamp = 0;
        // A DSU slot carries one IMU. The right controller is the default
        // physical source; this policy is intentionally separate from decoding.
        if (!legion_protocol::decode_report(
                report, legion_protocol::ControllerSide::Right, sample,
                timestamp))
            continue;

        sample.dt_seconds = have_timestamp_
                                ? legion_protocol::timestamp_delta_seconds(
                                      previous_timestamp_, timestamp)
                                : 0.0;
        previous_timestamp_ = timestamp;
        have_timestamp_ = true;
        return true;
    }
    return false;
}
} // namespace motion
