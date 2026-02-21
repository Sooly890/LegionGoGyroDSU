#include "iio.hpp"

#include <cstdint>
#include <iio.h>

#include <cstring>
#include <iostream>
#include <vector>

namespace iio
{
IIOMotion::IIOMotion()
    : ctx(iio_create_default_context()), gyro_chns(), accel_chns()
{
    if (!ctx)
    {
        error_bit = true;
        std::cerr << "Unable to create IIO context" << std::endl;
        return;
    }

    unsigned int ndev = iio_context_get_devices_count(ctx);
    std::cout << "all devices:" << std::endl;
    for (unsigned int i = 0; i < ndev; ++i)
    {
        struct iio_device* dev = iio_context_get_device(ctx, i);
        std::cout << iio_device_get_name(dev) << std::endl;
    }

    gyro_dev = iio_context_find_device(ctx, "gyro_3d");
    accel_dev = iio_context_find_device(ctx, "accel_3d");

    if (!gyro_dev)
    {
        error_bit = true;
        std::cerr << "Could not find gyro device. Look in README.md" << std::endl;
        return;
    }

    if (!accel_dev)
    {
        error_bit = true;
        std::cerr << "Could not find accel device. Look in README.md"
                  << std::endl;
        return;
    }

    std::cout << "\ngyro device: " << iio_device_get_name(gyro_dev) << std::endl;
    std::cout << "accel device: " << iio_device_get_name(accel_dev) << std::endl;

    // Helper lambda to print channels
    auto print_channels = [](struct iio_device* dev) {
        unsigned int nch = iio_device_get_channels_count(dev);
        for (unsigned int i = 0; i < nch; ++i)
        {
            struct iio_channel* channel = iio_device_get_channel(dev, i);
            std::cout << "channel " << iio_channel_get_id(channel)
                      << ", enabled: " << iio_channel_is_enabled(channel)
                      << std::endl;
        }
    };

    std::cout << "\ngyro channels:" << std::endl;
    print_channels(gyro_dev);

    std::cout << "\naccel channels:" << std::endl;
    print_channels(accel_dev);

    std::cout << std::endl;

    int index = 0;

    for (auto& name : gyro_chn_names)
    {
        struct iio_channel* ch =
            iio_device_find_channel(gyro_dev, name.c_str(), false);
        if (!ch)
        {
            continue;
        }

        iio_channel_enable(ch);

        gyro_chns[index++] = ch;

        char buf[32];

        ReadChannelAttr(ch, "scale", buf, sizeof(buf));
        double scale = atof(buf);

        ReadChannelAttr(ch, "sampling_frequency", buf, sizeof(buf));
        double freq = atof(buf);

        // starts off as milidegrees, but just in case the driver changes
        gyro_scale_[index - 1] = scale * rad2deg;
        gyro_freq_[index - 1] = freq;

        std::cout << "gyro " << name << " scale = " << scale
                  << ", sampling_frequency = " << freq
                  << " hz, enabled = " << iio_channel_is_enabled(ch) << std::endl;
    }

    index = 0;

    for (auto& name : accel_chn_names)
    {
        struct iio_channel* ch =
            iio_device_find_channel(accel_dev, name.c_str(), false);
        if (!ch)
        {
            continue;
        }

        iio_channel_enable(ch);

        accel_chns[index++] = ch;

        char buf[32];

        ReadChannelAttr(ch, "scale", buf, sizeof(buf));
        double scale = atof(buf);

        ReadChannelAttr(ch, "sampling_frequency", buf, sizeof(buf));
        double freq = atof(buf);

        accel_scale_[index - 1] = scale / g2m_s2;
        accel_freq_[index - 1] = freq;

        std::cout << "accel " << name << " scale = " << scale
                  << ", sampling_frequency = " << freq
                  << " hz, enabled = " << iio_channel_is_enabled(ch) << std::endl;
    }

    gyro_buf = iio_device_create_buffer(gyro_dev, 1, false);
    accel_buf = iio_device_create_buffer(accel_dev, 1, false);

    Update();

    std::cout << "Sample size gyro: " << iio_device_get_sample_size(gyro_dev)
              << std::endl;
    std::cout << "Sample size accel: " << iio_device_get_sample_size(accel_dev)
              << std::endl;
}
IIOMotion::~IIOMotion()
{
    iio_buffer_destroy(gyro_buf);
    iio_buffer_destroy(accel_buf);
    iio_context_destroy(ctx);
}

void IIOMotion::ReadChannelAttr(iio_channel* chn, std::string attr, char buf[],
                                size_t buf_len)
{
    ssize_t attr_len = iio_channel_attr_read(chn, attr.c_str(), buf, buf_len);
    if (attr_len > 0)
    {
        buf[attr_len] = '\0';
    }
}
void IIOMotion::ReadChannelValue(iio_channel* chn, iio_buffer* buffer,
                                 char value[], size_t value_len)
{
    size_t buffer_len = iio_channel_read(chn, buffer, value, value_len);
}

void IIOMotion::Update()
{
    iio_buffer_refill(gyro_buf);
    iio_buffer_refill(accel_buf);
}

auto IIOMotion::GetGyro() -> Vec3
{
    const double gyroDeadzone = 0.16;
    return GeneralRead(gyro_chns, gyro_buf, gyro_scale_, gyroDeadzone);
}
auto IIOMotion::GetAccel() -> Vec3
{
    const double accelDeadzone = 0.012;
    return GeneralRead(accel_chns, accel_buf, accel_scale_, accelDeadzone);
}

auto IIOMotion::GeneralRead(iio_channel* chns[3], iio_buffer* buffer, Vec3 scale,
                            double deadzone) -> Vec3
{
    Vec3 ret{};

    for (int i = 0; i < 3; i++)
    {
        struct iio_channel* ch = chns[i];

        char value[4];
        ReadChannelValue(ch, buffer, value, sizeof(value));

        int32_t raw;
        std::memcpy(&raw, value, sizeof(raw));

        ret[i] = (double)raw * scale[i];

        if (std::abs(ret[i]) < deadzone)
        {
            ret[i] = 0.0;
        }
    }

    return ret;
}

} // namespace iio
