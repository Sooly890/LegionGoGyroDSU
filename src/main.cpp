#include "dsu.hpp"

#include <asio/io_context.hpp>

#include <iostream>

#include <chrono>
#include <deque>
#include <thread>

#include "iio.hpp"
#include "iio_to_dsu.hpp"

#include "setting.hpp"

#include <atomic>

std::atomic<bool> running{true};

void sigint_handler(int)
{
    running = false;
}

auto main() -> int
{
    std::signal(SIGINT, sigint_handler);

    const char* port_str = std::getenv("LGSDSU_PORT");
    const char* ip_str = std::getenv("LGSDSU_IP");
    const char* gyro_matrix_cstr = std::getenv("LGSDSU_GYRO_MATRIX");
    const char* accel_matrix_cstr = std::getenv("LGSDSU_ACCEL_MATRIX");

    int port = 26760;
    std::string ip = "127.0.0.1";

    std::string default_gyro_matrix_str = "-x,-y,z";
    std::string default_accel_matrix_str = "x,z,-y";

    auto default_gyro_matrix = IIOToDSU(default_gyro_matrix_str, nullptr);
    auto default_accel_matrix = IIOToDSU(default_accel_matrix_str, nullptr);

    auto gyro_matrix =
        IIOToDSU(std::string(gyro_matrix_cstr != nullptr ? gyro_matrix_cstr : ""),
                 &default_gyro_matrix);

    auto accel_matrix =
        IIOToDSU(std::string(accel_matrix_cstr != nullptr ? accel_matrix_cstr
                                                          : ""),
                 &default_accel_matrix);

    if (port_str != nullptr)
    {
        port = strtoll(port_str, nullptr, 10);
        std::cout << "Using port " << port << " and not " << 26760 << std::endl;
    }

    if (ip_str != nullptr)
    {
        ip = ip_str;
        std::cout << "Using IP " << ip << " and not " << "127.0.0.1" << std::endl;
    }

    if (gyro_matrix_cstr != nullptr)
    {
        std::cout << "Using gyro matrix (or default if it's invalid): "
                  << gyro_matrix_cstr << std::endl;
    }

    if (accel_matrix_cstr != nullptr)
    {
        std::cout << "Using accel matrix (or default if it's invalid): "
                  << accel_matrix_cstr << std::endl;
    }

    asio::io_context ioc;
    auto server = dsu::DSUServer(ioc, ip, port);

    auto& controller0 =
        server.controllers->controllerData[0].actualControllerData;
    auto& controller0Shared =
        server.controllers->controllerData[0].actualControllerData.sharedData;

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

#if POS_LOG
    iio::Vec3 debug_global_gyro;
#endif

    while (!iio.error_bit && running)
    {
        iio.Update();

        auto now = std::chrono::high_resolution_clock::now();
        double deltaTime =
            std::chrono::duration<double>(now - frame_start).count();
        frame_start = now;

        iio::Vec3 gyro = iio.GetGyro();
        iio::Vec3 accel = iio.GetAccel();

        // obsolete hardcoded matrix
        /*double old_gyro_x = gyro.x;
        double old_gyro_y = gyro.y;
        double old_gyro_z = gyro.z;

        gyro.x = -old_gyro_x;
        gyro.y = -old_gyro_y;
        gyro.z = old_gyro_z;

        double old_accel_x = accel.x;
        double old_accel_y = accel.y;
        double old_accel_z = accel.z;

        accel.x = old_accel_x;
        accel.y = old_accel_z;
        accel.z = -old_accel_y;*/

        gyro = gyro_matrix.Convert(gyro);
        accel = accel_matrix.Convert(accel);

#if POS_LOG
        debug_global_gyro += gyro * deltaTime;

        std::cout << "Gyro: " << gyro.x << ", " << gyro.y << ", " << gyro.z
                  << std::endl;
        std::cout << "Debug Global Gyro: " << debug_global_gyro.x << ", "
                  << debug_global_gyro.y << ", " << debug_global_gyro.z
                  << std::endl;
        std::cout << "Accel: " << accel.x << ", " << accel.y << ", " << accel.z
                  << std::endl;
        std::cout << "Delta Time: " << deltaTime << std::endl;
#endif

        bool gyro_changed = gyro.x != controller0gyro_x ||
                            gyro.y != controller0gyro_y ||
                            gyro.z != controller0gyro_z;

        bool accel_changed = accel.x != controller0.accelerometer_x ||
                             accel.y != controller0.accelerometer_y ||
                             accel.z != controller0.accelerometer_z;

        if (gyro_changed || accel_changed)
        {
            {
                std::lock_guard lock(server.controllers->sensor_mutex);
                controller0gyro_x = gyro.x;
                controller0gyro_y = gyro.y;
                controller0gyro_z = gyro.z;

                if (accel_changed)
                {
                    controller0.motion_data_timestamp_microseconds =
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            now - epoch)
                            .count();
                    controller0.accelerometer_x = accel.x;
                    controller0.accelerometer_y = accel.y;
                    controller0.accelerometer_z = accel.z;
                }
            }
            asio::post(ioc, [&server]() { server.Update(); });
        }
    }

    return 0;
}
