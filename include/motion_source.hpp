#pragma once

namespace motion
{
struct Vec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3() = default;
    Vec3(double x_value, double y_value, double z_value)
        : x(x_value), y(y_value), z(z_value)
    {
    }

    auto operator[](int index) -> double&;
    auto operator[](int index) const -> const double&;
    auto operator+(const Vec3& other) const -> Vec3;
    auto operator-(const Vec3& other) const -> Vec3;
    auto operator*(const Vec3& other) const -> Vec3;
    auto operator*(double scalar) const -> Vec3;
    auto operator/(double scalar) const -> Vec3;
    void operator+=(const Vec3& other);
};

struct MotionSample
{
    Vec3 gyro;
    Vec3 accel;
    double dt_seconds = 0.0;
};

class MotionSource
{
  public:
    virtual bool initialize() = 0;
    virtual bool poll(MotionSample& sample) = 0;
    virtual ~MotionSource() = default;
};
} // namespace motion
