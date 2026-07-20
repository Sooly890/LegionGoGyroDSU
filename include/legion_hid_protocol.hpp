#pragma once

#include "motion_source.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace motion::legion_protocol
{
constexpr std::size_t ReportSize = 64;
using Report = std::array<std::uint8_t, ReportSize>;
using Command = std::vector<std::uint8_t>;

enum class ControllerSide
{
    Left,
    Right,
};

bool is_supported_product(std::uint16_t product_id);
auto initialization_commands() -> std::vector<Command>;
bool decode_report(const Report& report, ControllerSide side,
                   MotionSample& sample, std::uint8_t& timestamp);
auto timestamp_delta_seconds(std::uint8_t previous, std::uint8_t current)
    -> double;
} // namespace motion::legion_protocol
