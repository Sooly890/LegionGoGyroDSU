#include "iio_motion_source.hpp"

#include "iio.hpp"

namespace motion
{
IIOMotionSource::~IIOMotionSource() = default;

bool IIOMotionSource::initialize()
{
    motion_ = std::make_unique<iio::IIOMotion>();
    return !motion_->error_bit;
}

bool IIOMotionSource::poll(MotionSample& sample)
{
    if (!motion_ || motion_->error_bit)
        return false;

    motion_->Update();
    const auto gyro = motion_->GetGyro();
    const auto accel = motion_->GetAccel();
    sample.gyro = {gyro.x, gyro.y, gyro.z};
    sample.accel = {accel.x, accel.y, accel.z};
    return !motion_->error_bit;
}
} // namespace motion
