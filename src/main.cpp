#include "dsu.hpp"

#include <asio/io_context.hpp>

#include <iostream>

#include <chrono>
#include <deque>
#include <thread>

#include "iio.hpp"

#define POS_LOG 0
#define PACKET_LOG 0

auto main() -> int
{
    const char* port_str = std::getenv("LGSDSU_PORT");

    int port = 26760;

    if (port_str)
    {
        port = std::stoi(port_str, nullptr, 10);
        std::cout << "Using port " << port << " and not " << 26760 << std::endl;
    }

    asio::io_context ioc;
    auto server = dsu::DSUServer(ioc, "0.0.0.0", port);

    auto& controller0 = server.controllers.controllerData[0].actualControllerData;
    auto& controller0Shared =
        server.controllers.controllerData[0].actualControllerData.sharedData;

    controller0Shared.state = dsu::outgoing::SlotState::Connected;
    controller0Shared.model = dsu::outgoing::DeviceModel::AllGyro;
    controller0Shared.connection = dsu::outgoing::ConnectionType::Bluetooth;
    controller0Shared.battery_status = dsu::outgoing::BatteryStatus::Full;

    // don't really need to do this as the dsu.cpp will auto do it, cause it's
    // dumb it's needed twice
    controller0.connected = 1;

    auto& controller0gyro_x = controller0.gyroscope_pitch;
    auto& controller0gyro_y =
        controller0.gyroscope_roll; // yes this is not a mistake
    auto& controller0gyro_z =
        controller0.gyroscope_yaw; // yaw and roll are flipped

    server.StartListeningThread();

    auto iio = iio::IIOMotion();

    const double sec2microseconds = 1000000;

    double freq_seconds = 1.0 / iio.GetMinFreq();
    double freq_microseconds = freq_seconds * sec2microseconds;

    auto epoch = std::chrono::high_resolution_clock::now();

    auto frame_start = std::chrono::high_resolution_clock::now();

    iio::Vec3 debug_global_gyro;

    while (true)
    {
        iio.Update();

        auto now = std::chrono::high_resolution_clock::now();
        double deltaTime =
            std::chrono::duration<double>(now - frame_start).count();
        frame_start = std::chrono::high_resolution_clock::now();

        iio::Vec3 gyro = iio.GetGyro();

        double old_gyro_x = gyro.x;
        double old_gyro_y = gyro.y;
        double old_gyro_z = gyro.z;

        gyro.x = -old_gyro_x;
        gyro.y = -old_gyro_y;
        gyro.z = old_gyro_z;

        iio::Vec3 accel = iio.GetAccel();

        /*accel.x = -accel.x;
        double old_accel_y = accel.y;
        accel.y = -accel.z;
        accel.z = -old_accel_y;*/

        double old_accel_x = accel.x;
        double old_accel_y = accel.y;
        double old_accel_z = accel.z;

        accel.x = old_accel_x;
        accel.y = old_accel_z;
        accel.z = -old_accel_y;

        debug_global_gyro += gyro * deltaTime;

#if POS_LOG

        // if (sample_count % 100 == 0)
        //{
        std::cout << "Gyro: " << gyro.x << ", " << gyro.y << ", " << gyro.z
                  << std::endl;
        std::cout << "Debug Global Gyro: " << debug_global_gyro.x << ", "
                  << debug_global_gyro.y << ", " << debug_global_gyro.z
                  << std::endl;
        std::cout << "Accel: " << accel.x << ", " << accel.y << ", " << accel.z
                  << std::endl;

        std::cout << "Delta Time: " << deltaTime << std::endl;
        //}
#endif

        bool gyro_changed = gyro.x != controller0gyro_x ||
                            gyro.y != controller0gyro_y ||
                            gyro.z != controller0gyro_z;

        bool accel_changed = false;

        if (accel.x != controller0.accelerometer_x ||
            accel.y != controller0.accelerometer_y ||
            accel.z != controller0.accelerometer_z)
        {
            controller0.motion_data_timestamp_microseconds =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock::now() - epoch)
                    .count();

            accel_changed = true;
        }

        if (gyro_changed || accel_changed)
        {
            controller0gyro_x = gyro.x;
            controller0gyro_y = gyro.y;
            controller0gyro_z = gyro.z;

            controller0.accelerometer_x = accel.x;
            controller0.accelerometer_y = accel.y;
            controller0.accelerometer_z = accel.z;

            server.Update();
        }
    }

    return 0;
}
