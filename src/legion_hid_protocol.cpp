#include "legion_hid_protocol.hpp"

#include <array>

namespace motion::legion_protocol
{
namespace
{
constexpr std::uint8_t MotionReportId = 0x74;
constexpr std::size_t LeftConnectionStatusOffset = 10;
constexpr std::size_t RightConnectionStatusOffset = 11;
constexpr std::uint8_t ConnectedStatusMask = 0x80;
constexpr double AccelerometerScale = 0.00212;
constexpr double GyroscopeScale = 0.001065;

// Controller-local orientation relative to the normalized motion frame. Keep
// these signs separate from sensor scaling and the user-configurable DSU
// matrices so they can be adjusted after validation on additional firmware.
constexpr std::array<double, 3> LeftAccelerometerSigns = {-1.0, -1.0, -1.0};
constexpr std::array<double, 3> LeftGyroscopeSigns = {-1.0, -1.0, -1.0};
constexpr std::array<double, 3> RightAccelerometerSigns = {-1.0, -1.0, 1.0};
constexpr std::array<double, 3> RightGyroscopeSigns = {-1.0, -1.0, 1.0};

// Assumption: one timestamp count represents 8 ms. Public captures establish
// the counter layout but do not formally specify its clock period.
constexpr double TimestampTickSeconds = 0.008;
constexpr double MaximumTimestampDeltaSeconds = 0.250;
constexpr double MinimumHostRatio = 0.25;
constexpr double MaximumHostRatio = 4.0;

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

bool is_known_gyro_glitch(std::int16_t value)
{
    return value == 254 || value == 255 || value == -254 || value == -255;
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

auto disable_high_quality_report(std::uint8_t controller) -> Command
{
    return {0x05, 0x06, 0x6A, 0x07, controller, 0x01, 0x01};
}
} // namespace

bool is_supported_product(std::uint16_t product_id)
{
    for (const auto supported : SupportedProducts)
        if (product_id == supported)
            return true;
    return false;
}

namespace
{
auto controller_id(ControllerSide side) -> std::uint8_t
{
    return side == ControllerSide::Left ? LeftController : RightController;
}
} // namespace

auto initialization_commands(ControllerSide side) -> std::vector<Command>
{
    const auto controller = controller_id(side);
    return {enable_imu(controller), enable_high_quality_report(controller)};
}

auto shutdown_commands(ControllerSide side) -> std::vector<Command>
{
    return {disable_high_quality_report(controller_id(side))};
}

bool decode_report(const Report& report, ControllerSide side,
                   MotionSample& sample, std::uint8_t& timestamp)
{
    if (report[0] != MotionReportId)
        return false;

    if (side == ControllerSide::Left)
    {
        timestamp = report[34];
        sample.accel = {
            read_be_i16(report, 35) * AccelerometerScale *
                LeftAccelerometerSigns[0],
            read_be_i16(report, 39) * AccelerometerScale *
                LeftAccelerometerSigns[1],
            read_be_i16(report, 37) * AccelerometerScale *
                LeftAccelerometerSigns[2]};
        sample.gyro = {
            read_be_i16(report, 41) * GyroscopeScale * LeftGyroscopeSigns[0],
            read_be_i16(report, 45) * GyroscopeScale * LeftGyroscopeSigns[1],
            read_be_i16(report, 43) * GyroscopeScale * LeftGyroscopeSigns[2]};
    } else
    {
        timestamp = report[47];
        sample.accel = {
            read_be_i16(report, 50) * AccelerometerScale *
                RightAccelerometerSigns[0],
            read_be_i16(report, 52) * AccelerometerScale *
                RightAccelerometerSigns[1],
            read_be_i16(report, 48) * AccelerometerScale *
                RightAccelerometerSigns[2]};
        sample.gyro = {
            read_be_i16(report, 56) * GyroscopeScale * RightGyroscopeSigns[0],
            read_be_i16(report, 58) * GyroscopeScale * RightGyroscopeSigns[1],
            read_be_i16(report, 54) * GyroscopeScale * RightGyroscopeSigns[2]};
    }
    return true;
}

bool has_known_gyro_glitch(const Report& report, ControllerSide side)
{
    if (report[0] != MotionReportId)
        return false;

    const std::array<std::size_t, 3> offsets =
        side == ControllerSide::Left
            ? std::array<std::size_t, 3>{41, 43, 45}
            : std::array<std::size_t, 3>{54, 56, 58};
    for (const auto offset : offsets)
        if (is_known_gyro_glitch(read_be_i16(report, offset)))
            return true;
    return false;
}

auto connected_controller_side(const Report& report)
    -> std::optional<ControllerSide>
{
    if (report[0] != MotionReportId)
        return std::nullopt;

    // The right controller is preferred when both controllers are connected.
    if ((report[RightConnectionStatusOffset] & ConnectedStatusMask) != 0)
        return ControllerSide::Right;
    if ((report[LeftConnectionStatusOffset] & ConnectedStatusMask) != 0)
        return ControllerSide::Left;
    return std::nullopt;
}

auto timestamp_delta_seconds(std::uint8_t previous, std::uint8_t current)
    -> double
{
    const auto ticks = static_cast<std::uint8_t>(current - previous);
    return static_cast<double>(ticks) * TimestampTickSeconds;
}

bool is_plausible_timestamp_delta(double device_delta_seconds,
                                  double host_delta_seconds)
{
    if (device_delta_seconds <= 0.0 ||
        device_delta_seconds > MaximumTimestampDeltaSeconds)
        return false;
    if (host_delta_seconds <= 0.0)
        return true;

    const auto ratio = device_delta_seconds / host_delta_seconds;
    return ratio >= MinimumHostRatio && ratio <= MaximumHostRatio;
}
} // namespace motion::legion_protocol
