#pragma once

#include "motion_source.hpp"

#include <memory>

namespace iio
{
class IIOMotion;
}

namespace motion
{
class IIOMotionSource final : public MotionSource
{
  public:
    IIOMotionSource() = default;
    ~IIOMotionSource() override;

    bool initialize() override;
    bool poll(MotionSample& sample) override;

  private:
    std::unique_ptr<iio::IIOMotion> motion_;
};
} // namespace motion
