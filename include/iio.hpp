#pragma once

#include <algorithm>
#include <cmath>
#include <iio.h>

#include <iostream>
#include <string>
#include <vector>

namespace iio
{
struct Vec3
{
    double x;
    double y;
    double z;

    Vec3(double x, double y, double z) : x(x), y(y), z(z)
    {
    }

    Vec3() : x(0), y(0), z(0)
    {
    }

    auto operator+(const Vec3& other) const -> Vec3
    {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    auto operator-(const Vec3& other) const -> Vec3
    {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    void operator+=(const Vec3& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
    }

    auto operator*(const Vec3& other) const -> Vec3
    {
        return Vec3(x * other.x, y * other.y, z * other.z);
    }

    auto operator*(const double& other) const -> Vec3
    {
        return Vec3(x * other, y * other, z * other);
    }

    auto operator/(const double& other) const -> Vec3
    {
        return Vec3(x / other, y / other, z / other);
    }

    auto operator[](int index) -> double&
    {
        switch (index)
        {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        default:
            std::cout << "Invalid index" << std::endl;
            return x;
        }
    }

    auto operator[](int index) const -> const double&
    {
        switch (index)
        {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        default:
            std::cout << "Invalid index" << std::endl;
            return x;
        }
    }
};

class IIOMotion
{
  public:
    IIOMotion();
    ~IIOMotion();

    bool error_bit = false;

    void Update();

    auto GetGyro() -> Vec3;
    auto GetAccel() -> Vec3;

    auto GetMinFreq() const -> double
    {
        // ugly but functional
        return std::min(std::min(std::min(gyro_freq_.x, gyro_freq_.y),
                                 std::min(accel_freq_.x, accel_freq_.y)),
                        std::min(gyro_freq_.z, accel_freq_.z));
    }

  private:
    static void ReadChannelAttr(iio_channel* chn, const std::string& attr,
                                char buf[], size_t buf_len);

    static void ReadChannelValue(iio_channel* chn, iio_buffer* buffer,
                                 char value[], size_t value_len);

    static auto GeneralRead(iio_channel* chns[3], iio_buffer* buffer,
                            const Vec3& scale, double deadzone) -> Vec3;

    iio_context* ctx;
    iio_device* gyro_dev;
    iio_device* accel_dev;

    iio_channel* gyro_chns[3];
    iio_channel* accel_chns[3];

    Vec3 gyro_scale_;
    Vec3 accel_scale_;

    Vec3 gyro_freq_;
    Vec3 accel_freq_;

    iio_buffer* gyro_buf;
    iio_buffer* accel_buf;

    inline static const std::vector<std::string> gyro_chn_names = {"anglvel_x",
                                                                   "anglvel_y",
                                                                   "anglvel_z"};

    inline static const std::vector<std::string> accel_chn_names = {"accel_x",
                                                                    "accel_y",
                                                                    "accel_z"};

    inline static const double rad2deg = 57.29578;
    inline static const double g2m_s2 = 9.80665;
};
} // namespace iio
