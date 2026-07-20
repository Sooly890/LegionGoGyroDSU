#include "legion_hid_protocol.hpp"

#include <array>

namespace motion::legion_protocol
{
namespace
{
constexpr std::uint8_t MotionReportId = 0x74;
constexpr double AccelerometerScale = 0.00212;
constexpr double GyroscopeScale = 0.001065;

// Assumption: one timestamp count represents 8 ms. Public captures establish
// the counter layout but do not formally specify its clock period.
constexpr double TimestampTickSeconds = 0.008;

constexpr std::uint8_t LeftController = 0x03;
constexpr std::uint8_t RightController = 0x04;

constexpr std::array<std::uint16_t, 8> SupportedProducts = {
    0x6182, 0x6183, 0x6184, 0x6185, 0x61EB, 0x61EC, 0x61ED, 0x61EE};

auto read_be_i16(const Report& report, std::size_t offset) -> std::int16_t
{
    const auto value = static_cast<std::uint16_t>(report[offset]) << 8U |
                       static_cast<std::uint16_t>(report[offset + 1]);
    return static_cast<std::int16_t>(value);
}

auto enable_imu(std::uint8_t controller) -> Command
{
    return {0x05, 0x06, 0x6A, 0x02, controller, 0x01, 0x01};
}

auto enable_high_quality_report(std::uint8_t controller) -> Command
{
    // Assumption: command 0x6A/subcommand 0x07 selects the high-rate report,
    // and mode 0x02 enables it. Keep this packet here until verified on Lenovo
    // controller firmware independently of existing implementations.
    return {0x05, 0x06, 0x6A, 0x07, controller, 0x02, 0x01};
}
} // namespace

bool is_supported_product(std::uint16_t product_id)
{
    for (const auto supported : SupportedProducts)
        if (product_id == supported)
            return true;
    return false;
}

auto initialization_commands() -> std::vector<Command>
{
    return {enable_imu(LeftController),
            enable_high_quality_report(LeftController),
            enable_imu(RightController),
            enable_high_quality_report(RightController)};
}

bool decode_report(const Report& report, ControllerSide side,
                   MotionSample& sample, std::uint8_t& timestamp)
{
    if (report[0] != MotionReportId)
        return false;

    if (side == ControllerSide::Left)
    {
        timestamp = report[34];
        sample.accel = {read_be_i16(report, 35) * AccelerometerScale,
                        read_be_i16(report, 39) * AccelerometerScale,
                        read_be_i16(report, 37) * AccelerometerScale};
        sample.gyro = {read_be_i16(report, 41) * GyroscopeScale,
                       read_be_i16(report, 45) * GyroscopeScale,
                       read_be_i16(report, 43) * GyroscopeScale};
    } else
    {
        timestamp = report[47];
        sample.accel = {read_be_i16(report, 50) * AccelerometerScale,
                        read_be_i16(report, 52) * AccelerometerScale,
                        read_be_i16(report, 48) * AccelerometerScale};
        sample.gyro = {read_be_i16(report, 56) * GyroscopeScale,
                       read_be_i16(report, 58) * GyroscopeScale,
                       read_be_i16(report, 54) * GyroscopeScale};
    }
    return true;
}

auto timestamp_delta_seconds(std::uint8_t previous, std::uint8_t current)
    -> double
{
    const auto ticks = static_cast<std::uint8_t>(current - previous);
    return static_cast<double>(ticks) * TimestampTickSeconds;
}
} // namespace motion::legion_protocol
