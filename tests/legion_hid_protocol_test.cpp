#include "legion_hid_protocol.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>

namespace
{
void put_be_i16(motion::legion_protocol::Report& report, std::size_t offset,
                std::int16_t value)
{
    const auto bits = static_cast<std::uint16_t>(value);
    report[offset] = static_cast<std::uint8_t>(bits >> 8U);
    report[offset + 1] = static_cast<std::uint8_t>(bits & 0xFFU);
}

bool near(double actual, double expected)
{
    return std::abs(actual - expected) < 0.0000001;
}
} // namespace

auto main() -> int
{
    using namespace motion::legion_protocol;

    Report report{};
    report[0] = 0x74;
    report[47] = 0xFE;
    put_be_i16(report, 48, -100);
    put_be_i16(report, 50, 200);
    put_be_i16(report, 52, -300);
    put_be_i16(report, 54, 400);
    put_be_i16(report, 56, -500);
    put_be_i16(report, 58, 600);

    motion::MotionSample sample;
    std::uint8_t timestamp = 0;
    assert(decode_report(report, ControllerSide::Right, sample, timestamp));
    assert(timestamp == 0xFE);
    assert(near(sample.accel.x, 200 * 0.00212));
    assert(near(sample.accel.y, -300 * 0.00212));
    assert(near(sample.accel.z, -100 * 0.00212));
    assert(near(sample.gyro.x, -500 * 0.001065));
    assert(near(sample.gyro.y, 600 * 0.001065));
    assert(near(sample.gyro.z, 400 * 0.001065));

    assert(near(timestamp_delta_seconds(0xFE, 0x02), 4 * 0.008));
    assert(is_supported_product(0x6182));
    assert(is_supported_product(0x61EE));
    assert(!is_supported_product(0x1234));

    report[0] = 0x01;
    assert(!decode_report(report, ControllerSide::Right, sample, timestamp));
    return 0;
}
